$serverIp = "127.0.0.1"
$port = 6379
$totalRequests = 10000

Write-Host "Connecting to database at $serverIp`:$port..."
try {
    $client = New-Object System.Net.Sockets.TcpClient($serverIp, $port)
    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $reader = New-Object System.IO.StreamReader($stream)
} catch {
    Write-Host "Failed to connect! Is the server running?" -ForegroundColor Red
    exit
}

Write-Host "Connected! Blasting $totalRequests SET commands..." -ForegroundColor Cyan

# Start the timer!
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

for ($i = 0; $i -lt $totalRequests; $i++) {
    # 1. Send the command
    $writer.WriteLine("SET key$i value$i")
    $writer.Flush()
    
    # 2. Wait for the +OK response to ensure it was processed
    $response = $reader.ReadLine()
}

# Stop the timer!
$stopwatch.Stop()
$client.Close()

$timeInSeconds = $stopwatch.Elapsed.TotalSeconds
$requestsPerSecond = [math]::Round($totalRequests / $timeInSeconds, 2)

Write-Host "`n--- BENCHMARK RESULTS ---" -ForegroundColor Yellow
Write-Host "Total Requests:  $totalRequests"
Write-Host "Total Time:      $($timeInSeconds) seconds"
Write-Host "Throughput:      $requestsPerSecond Requests / Second" -ForegroundColor Green
Write-Host "Average Latency: $([math]::Round(($timeInSeconds / $totalRequests) * 1000, 3)) ms per request" -ForegroundColor Green
