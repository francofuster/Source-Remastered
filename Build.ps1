<#
    Build.ps1 -- Compila los tres QVM y verifica que lleguen al build.

    Por que existe:
      Los .bat de compilacion saltan a la etiqueta :quit ante un error, borran los
      .asm y terminan con codigo 0. Es decir, una compilacion fallida se ve igual
      que una exitosa desde afuera, y el juego sigue cargando el QVM viejo sin que
      nada lo avise. Este script compara la fecha de cada .qvm antes y despues, que
      es la unica forma confiable de saber si realmente se regenero.

    Uso:
      .\Build.ps1            compila los tres
      .\Build.ps1 game       compila solo game (tambien: cgame, ui)
      .\Build.ps1 -NoPause   no espera Enter al final (para scripts/CI)
#>

param(
    [ValidateSet('all','game','cgame','ui')]
    [string]$Target = 'all',
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'
$root    = $PSScriptRoot
$vmDir   = Join-Path (Split-Path $root -Parent) 'Build-Remastered\ZEQ2\vm'

$modules = @(
    [pscustomobject]@{ Name='cgame'; Dir='Game\CGame'; Bat='cgame.bat'; Qvm='cgame.qvm' },
    [pscustomobject]@{ Name='game';  Dir='Game\Game';  Bat='game.bat';  Qvm='game.qvm'  },
    [pscustomobject]@{ Name='ui';    Dir='Game\UI';    Bat='ui.bat';    Qvm='ui.qvm'    }
)

if ($Target -ne 'all') {
    $modules = $modules | Where-Object { $_.Name -eq $Target }
}


# ---------------------------------------------------------------------------
# Guardia de orden de inicializadores posicionales.
#
# Por que existe: botlite_profile_t y botlite_combat_policy_t se inicializan por
# POSICION en g_botlite_profile.c. Si alguien agrega un campo al struct en un
# lugar y el valor en otro, el compilador no dice nada: los tipos son casi todos
# int/float/qboolean y encajan igual. El resultado es que cada campo a partir de
# ahi toma el valor del vecino, en silencio.
#
# Paso exactamente eso dos veces:
#   - chargeCommitCooldown / stunChargeHoldMs quedaron en el medio del
#     inicializador y al final del struct: 20 campos corridos dos lugares.
#   - allowsDodgeIncoming y allowsOffensiveBreakLimit quedaron invertidos.
# Los .cfg tapaban ambos casos porque fijan esas claves por nombre, asi que ni
# siquiera se notaba en juego.
#
# Esta comprobacion usa los comentarios /* nombreDeCampo */ del inicializador
# como declaracion de intencion y los contrasta contra el orden real del struct.
# Solo valida las entradas que tienen comentario; las de arriba no lo tienen.
# ---------------------------------------------------------------------------

function Get-StructFieldOrder {
    param([string]$Text, [string]$StructName)

    $pattern = '(?s)typedef struct \{(((?!typedef struct \{).)*?)\} ' + [regex]::Escape($StructName) + ';'
    $m = [regex]::Match($Text, $pattern)
    if (-not $m.Success) { return $null }

    $fields = @()
    foreach ($line in ($m.Groups[1].Value -split "`n")) {
        if ($line -match '^\s*(?:float|int|qboolean)\s+(\w+)\s*;') {
            $fields += $Matches[1]
        }
    }
    return ,$fields
}

function Get-InitializerEntries {
    param([string]$Text, [string]$InitName)

    $pattern = '(?s)' + [regex]::Escape($InitName) + '\s*=\s*\{(.*?)\r?\n\};'
    $m = [regex]::Match($Text, $pattern)
    if (-not $m.Success) { return $null }

    $entries = @()
    foreach ($line in ($m.Groups[1].Value -split "`n")) {
        $trimmed = $line.Trim()
        if ($trimmed -eq '') { continue }
        # Linea que es solo comentario: no es una entrada del inicializador.
        if ($trimmed -match '^/\*' -and $trimmed -notmatch '^\S+\s*,') { continue }
        if ($trimmed -match '^\*') { continue }

        $comment = ''
        if ($trimmed -match '/\*\s*(\w+)') { $comment = $Matches[1] }
        $entries += [pscustomobject]@{ Text = $trimmed; Comment = $comment }
    }
    return ,$entries
}

function Test-InitializerOrder {
    param([string]$HeaderText, [string]$SourceText, [string]$StructName, [string[]]$InitNames)

    $problems = @()
    $fields = Get-StructFieldOrder -Text $HeaderText -StructName $StructName
    if ($null -eq $fields) {
        $problems += "No se pudo leer el struct $StructName del header."
        return $problems
    }

    foreach ($initName in $InitNames) {
        $entries = Get-InitializerEntries -Text $SourceText -InitName $initName
        if ($null -eq $entries) {
            $problems += "No se pudo leer el inicializador $initName."
            continue
        }
        if ($entries.Count -ne $fields.Count) {
            $problems += ("{0}: {1} valores contra {2} campos de {3}." -f $initName, $entries.Count, $fields.Count, $StructName)
            continue
        }
        for ($i = 0; $i -lt $entries.Count; $i++) {
            $comment = $entries[$i].Comment
            if ($comment -eq '') { continue }
            if ($comment -ne $fields[$i]) {
                $problems += ("{0}: posicion {1} dice /* {2} */ pero el struct tiene '{3}'." -f $initName, $i, $comment, $fields[$i])
            }
        }
    }
    return $problems
}

$headerPath = Join-Path $root 'Game\Game\g_botlite.h'
$profilePath = Join-Path $root 'Game\Game\g_botlite_profile.c'

if ((Test-Path $headerPath) -and (Test-Path $profilePath)) {
    $headerText  = Get-Content $headerPath -Raw
    $profileText = Get-Content $profilePath -Raw

    $issues = @()
    $issues += Test-InitializerOrder -HeaderText $headerText -SourceText $profileText `
        -StructName 'botlite_profile_t' `
        -InitNames @('botlite_skill1_profile_default','botlite_skill2_profile_default','botlite_skill3_profile_default')
    $issues += Test-InitializerOrder -HeaderText $headerText -SourceText $profileText `
        -StructName 'botlite_combat_policy_t' `
        -InitNames @('botlite_skill1_policy_default','botlite_skill2_policy_default','botlite_skill3_policy_default')

    $issues = @($issues | Where-Object { $_ -ne $null -and $_ -ne '' })
    if ($issues.Count -gt 0) {
        Write-Host ""
        Write-Host "Los inicializadores posicionales no coinciden con el struct:" -ForegroundColor Red
        foreach ($issue in $issues) { Write-Host ("  " + $issue) -ForegroundColor Red }
        Write-Host ""
        Write-Host "Compilaria igual y cada campo desde ahi tomaria el valor del vecino." -ForegroundColor Yellow
        Write-Host "Se aborta antes de compilar." -ForegroundColor Yellow
        Write-Host ""
        if (-not $NoPause) {
            Write-Host "Enter para cerrar..." -NoNewline
            [void](Read-Host)
        }
        exit 1
    }
    Write-Host "Inicializadores de botlite: orden verificado." -ForegroundColor DarkGray
}
if (-not (Test-Path $vmDir)) {
    New-Item -ItemType Directory -Force -Path $vmDir | Out-Null
    Write-Host "Creada carpeta de salida: $vmDir" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Salida: $vmDir" -ForegroundColor DarkGray
Write-Host ""

$results = @()

foreach ($m in $modules) {
    $qvmPath = Join-Path $vmDir $m.Qvm
    $before  = $null
    if (Test-Path $qvmPath) { $before = (Get-Item $qvmPath).LastWriteTime }

    Write-Host ("Compilando {0}..." -f $m.Name) -ForegroundColor Cyan

    # Los .bat asumen su propia carpeta como working dir y usan rutas relativas.
    # Set-Location no alcanza: en PS 5.1 los procesos hijos heredan el CWD original
    # del proceso, no el de PowerShell. Hay que pasar -WorkingDirectory explicito.
    $workDir = Join-Path $root $m.Dir
    $batPath = Join-Path $workDir $m.Bat
    # Ruta absoluta al .bat a proposito: en este equipo cmd no resuelve el .bat
    # desde el directorio actual (NoDefaultCurrentDirectoryInExePath).
    $proc = Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', "`"$batPath`"" `
        -WorkingDirectory $workDir -NoNewWindow -Wait -PassThru

    $after = $null
    if (Test-Path $qvmPath) { $after = (Get-Item $qvmPath).LastWriteTime }

    if ($null -eq $after) {
        $status = 'FALLO (no se genero)'
        $ok = $false
    } elseif ($null -ne $before -and $after -le $before) {
        $status = 'FALLO (QVM sin cambios)'
        $ok = $false
    } else {
        $sizeKb = [math]::Round((Get-Item $qvmPath).Length / 1KB)
        $status = ("OK  {0} KB  {1}" -f $sizeKb, $after.ToString('HH:mm:ss'))
        $ok = $true
    }

    $results += [pscustomobject]@{ Modulo=$m.Name; Estado=$status; Ok=$ok }
}

Write-Host ""
Write-Host "-------- Resultado --------"
foreach ($r in $results) {
    if ($r.Ok) { $color = 'Green' } else { $color = 'Red' }
    Write-Host ("{0,-7} {1}" -f $r.Modulo, $r.Estado) -ForegroundColor $color
}
Write-Host ""

$failed = @($results | Where-Object { -not $_.Ok })
if ($failed.Count -gt 0) {
    Write-Host "Hubo errores de compilacion. Para ver el detalle, corre el .bat" -ForegroundColor Red
    Write-Host "del modulo que fallo directamente desde su carpeta, por ejemplo:" -ForegroundColor Red
    Write-Host "  cd Game\Game ; .\game.bat" -ForegroundColor Red
    Write-Host ""
}

if (-not $NoPause) {
    Write-Host "Enter para cerrar..." -NoNewline
    [void](Read-Host)
}

if ($failed.Count -gt 0) { exit 1 }
exit 0
