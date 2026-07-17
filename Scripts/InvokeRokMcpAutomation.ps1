param(
    [Parameter(Mandatory = $true)]
    [string]$Action,

    [string]$PayloadJson = '{}',

    [int]$Port = 8395,

    [string]$RequestId = ([Guid]::NewGuid().ToString('N')),

    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'

function Send-WebSocketText {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [string]$Text
    )
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $segment = [ArraySegment[byte]]::new($bytes)
    $null = $Socket.SendAsync(
        $segment,
        [System.Net.WebSockets.WebSocketMessageType]::Text,
        $true,
        [System.Threading.CancellationToken]::None
    ).GetAwaiter().GetResult()
}

function Receive-WebSocketText {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [System.Threading.CancellationToken]$CancellationToken
    )
    $stream = [System.IO.MemoryStream]::new()
    try {
        do {
            $buffer = [byte[]]::new(65536)
            $segment = [ArraySegment[byte]]::new($buffer)
            $result = $Socket.ReceiveAsync($segment, $CancellationToken).GetAwaiter().GetResult()
            if ($result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
                throw "MCP WebSocket closed before a response was received"
            }
            if ($result.Count -gt 0) {
                $stream.Write($buffer, 0, $result.Count)
            }
        } while (-not $result.EndOfMessage)
        return [System.Text.Encoding]::UTF8.GetString($stream.ToArray())
    }
    finally {
        $stream.Dispose()
    }
}

$payload = $PayloadJson | ConvertFrom-Json -AsHashtable
$socket = [System.Net.WebSockets.ClientWebSocket]::new()
$socket.Options.AddSubProtocol('mcp-automation')
$timeout = [System.Threading.CancellationTokenSource]::new([TimeSpan]::FromSeconds($TimeoutSeconds))

try {
    $uri = [Uri]("ws://127.0.0.1:{0}/" -f $Port)
    $null = $socket.ConnectAsync($uri, $timeout.Token).GetAwaiter().GetResult()

    $hello = [ordered]@{
        type = 'bridge_hello'
        clientName = 'Codex'
        clientVersion = '1'
    } | ConvertTo-Json -Compress
    Send-WebSocketText -Socket $socket -Text $hello
    $ackText = Receive-WebSocketText -Socket $socket -CancellationToken $timeout.Token
    $ack = $ackText | ConvertFrom-Json
    if ($ack.type -ne 'bridge_ack') {
        throw "Unexpected MCP handshake response: $ackText"
    }

    $request = [ordered]@{
        type = 'automation_request'
        requestId = $RequestId
        action = $Action
        payload = $payload
    } | ConvertTo-Json -Depth 100 -Compress
    Send-WebSocketText -Socket $socket -Text $request

    while ($true) {
        $responseText = Receive-WebSocketText -Socket $socket -CancellationToken $timeout.Token
        $response = $responseText | ConvertFrom-Json
        if ($response.type -eq 'automation_response' -and $response.requestId -eq $RequestId) {
            $responseText
            break
        }
        if ($response.type -eq 'bridge_error') {
            throw "MCP bridge error: $responseText"
        }
    }
}
finally {
    if ($socket.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
        try {
            $null = $socket.CloseOutputAsync(
                [System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure,
                'done',
                [System.Threading.CancellationToken]::None
            ).GetAwaiter().GetResult()
        }
        catch {
            # The bridge may close immediately after sending its response.
        }
    }
    $timeout.Dispose()
    $socket.Dispose()
}
