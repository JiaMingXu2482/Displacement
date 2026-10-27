<#
.SYNOPSIS
  读取 S32K144 位移数据流（ASCII 文本行）。

.DESCRIPTION
  固件格式：每个采样点一行，十进制整数 + CRLF，单位 0.1 mm。
  例如 7500 表示 750.0 mm，-3 表示 -0.3 mm。
  串口：921600，8 数据位，无校验，1 停止位，无流控。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\read_displacement.ps1 -Port COM4

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\read_displacement.ps1 -Port COM4 -Seconds 10 -Save data.txt
#>
param(
    [string]$Port = "COM4",
    [int]$Baud = 921600,
    [int]$Seconds = 5,
    [string]$Save = ""
)

$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName = $Port
$sp.BaudRate = $Baud
$sp.Parity = [System.IO.Ports.Parity]::None
$sp.DataBits = 8
$sp.StopBits = [System.IO.Ports.StopBits]::One
$sp.Handshake = [System.IO.Ports.Handshake]::None
$sp.ReadTimeout = 1000
$sp.NewLine = "`r`n"

try {
    $sp.Open()
} catch {
    Write-Host "无法打开 $Port : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "可用串口：" -ForegroundColor Yellow
    [System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "已打开 $Port @ $Baud, 8N1" -ForegroundColor Cyan
Start-Sleep -Milliseconds 300
$sp.DiscardInBuffer()

$values = New-Object System.Collections.Generic.List[int]
$sw = [Diagnostics.Stopwatch]::StartNew()
$lines = New-Object System.Collections.Generic.List[string]

while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
    try {
        $line = $sp.ReadLine().Trim()
    } catch {
        continue
    }
    if ($line -match '^-?\d+$') {
        $values.Add([int]$line)
        $lines.Add($line)
    }
}
$sw.Stop()
$sp.Close()

if ($Save -ne "") {
    [System.IO.File]::WriteAllLines($Save, $lines)
    Write-Host "已保存 $($lines.Count) 行到 $Save" -ForegroundColor Green
}

if ($values.Count -eq 0) {
    Write-Host "没有解析到任何整数行。请确认：" -ForegroundColor Red
    Write-Host "  1. 板子已运行新固件（按 F5 下载后复位）"
    Write-Host "  2. 串口选对、波特率 921600"
    Write-Host "  3. 串口助手已关闭，没有占用端口"
    exit 1
}

$elapsed = $sw.Elapsed.TotalSeconds
$rate = $values.Count / $elapsed
Write-Host ""
Write-Host ("共 {0} 点，耗时 {1:N2} s，平均 {2:N0} 点/秒" -f $values.Count, $elapsed, $rate)
if ($rate -lt 4000) {
    Write-Host "警告：低于 5000 点/秒，检查串口缓冲或接收端性能。" -ForegroundColor Yellow
}

$n = [Math]::Min(20, $values.Count)
Write-Host ""
Write-Host "序号    原始值     毫米" -ForegroundColor Green
for ($i = 0; $i -lt $n; $i++) {
    Write-Host ("{0,5}  {1,8}  {2,10:N1}" -f $i, $values[$i], ($values[$i] / 10.0))
}

$min = ($values | Measure-Object -Minimum).Minimum
$max = ($values | Measure-Object -Maximum).Maximum
$avg = ($values | Measure-Object -Average).Average
Write-Host ""
Write-Host ("统计：最小 {0:N1} mm，最大 {1:N1} mm，平均 {2:N1} mm" -f ($min / 10.0), ($max / 10.0), ($avg / 10.0))
