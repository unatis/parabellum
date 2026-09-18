# Снимок игры без открытия редактора: запускает Parabellum в -game -RenderOffScreen,
# ставит игрока, поворачивает камеру, ждёт заданное время и пишет PNG в Saved/Screenshots.
#
# Это основной способ проверки: посмотреть на результат, ничего не открывая руками. Всё,
# что видно в игре - свет, поза оружия, прицельная линия, выброс гильз - проверяется отсюда.
#
# Примеры:
#   Tools\shot.ps1 -Name aim -Aim 1 -After 12
#   Tools\shot.ps1 -Name bench -Start PS4 -Yaw 90 -Exec "pbl.Bench.Fire"
#   Tools\shot.ps1 -Name cycle -Exec "pbl.Bench.Slow 0.05" -Delayed "pbl.Bench.Fire" -After 20
#
# Разрешение по умолчанию 1280x720 совпадает с DefaultGameUserSettings.ini: движок пишет
# скриншот в разрешении вьюпорта, и расхождение молча даёт не тот кадр.
param(
    [string]$Name,              # имя файла в Saved/Screenshots
    [string]$After = '15',      # через сколько секунд после старта снимать
    [string]$Start = 'PS2',     # тег PlayerStart
    [string]$Yaw = '0',
    [string]$Pitch = '',
    [string]$Exec = '',         # консольные команды при старте
    [string]$Delayed = '',      # консольные команды за секунду до снимка
    [string]$Ini = '',          # -ini:Секция:Ключ=Значение
    [string]$Ini2 = '',
    [string]$AutoFire = '',     # сделать N выстрелов до снимка
    [string]$Camera = '',       # имя камеры на уровне вместо камеры игрока
    [string]$Aim = '',          # 1 - снимать с прицеливания
    [string]$ResX = '1280',
    [string]$ResY = '720',
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [int]$TimeoutMs = 240000
)

$project = Join-Path (Split-Path -Parent $PSScriptRoot) 'Parabellum.uproject'
$a = @($project, '-game', '-RenderOffScreen', "-ResX=$ResX", "-ResY=$ResY", '-unattended', '-nosound',
       "-PBLScreenshotAfter=$After", "-PBLPlayerStart=$Start", "-PBLLookYaw=$Yaw",
       "-PBLScreenshotName=$Name", '-stdout')
if ($Pitch    -ne '') { $a += "-PBLLookPitch=$Pitch" }
if ($Exec     -ne '') { $a += "-ExecCmds=`"$Exec`"" }
if ($Ini      -ne '') { $a += "-ini:$Ini" }
if ($AutoFire -ne '') { $a += "-PBLAutoFire=$AutoFire" }
if ($Ini2     -ne '') { $a += "-ini:$Ini2" }
if ($Camera   -ne '') { $a += "-PBLScreenshotCamera=$Camera" }
if ($Delayed  -ne '') { $a += "-PBLExecDelayed=`"$Delayed`"" }
if ($Aim      -ne '') { $a += '-PBLAim' }

$p = Start-Process -FilePath $Engine -ArgumentList $a -PassThru -WindowStyle Hidden
$p.WaitForExit($TimeoutMs) | Out-Null
if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -Confirm:$false
    "${Name}: KILLED (timeout ${TimeoutMs} ms)"
} else {
    "${Name}: exit $($p.ExitCode)"
}
