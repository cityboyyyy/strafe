param([string]$KeyDir = (Join-Path $env:USERPROFILE ".controller-signing"), [string]$PublicOut)

$ErrorActionPreference = "Continue"
$openssl = (Get-Command openssl -ErrorAction SilentlyContinue).Source
if (-not $openssl) { $openssl = "C:\Program Files\Git\mingw64\bin\openssl.exe" }
New-Item -ItemType Directory -Force $KeyDir | Out-Null

$key = Join-Path $KeyDir "driver_key.pem"
$cert = Join-Path $KeyDir "driver_cert.pem"
$pfx = Join-Path $KeyDir "driver_signing.pfx"
$der = Join-Path $KeyDir "controller_driver.cer"
$passFile = Join-Path $KeyDir "pfx_password.txt"

if (-not (Test-Path $pfx)) {
    & $openssl req -x509 -newkey rsa:3072 -sha256 -days 3650 -nodes -keyout $key -out $cert `
        -subj "/CN=Controller Emulator Driver/O=Controller Emulator" `
        -addext "basicConstraints=critical,CA:FALSE" `
        -addext "keyUsage=critical,digitalSignature" `
        -addext "extendedKeyUsage=codeSigning" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "certificate creation failed" }
    $password = [guid]::NewGuid().ToString("N")
    Set-Content -Path $passFile -Value $password -NoNewline -Encoding ascii
    & $openssl pkcs12 -export -out $pfx -inkey $key -in $cert -passout "pass:$password" `
        -keypbe AES-256-CBC -certpbe AES-256-CBC -macalg sha256 2>&1 | Out-Null
    & $openssl x509 -in $cert -outform der -out $der 2>&1 | Out-Null
    Remove-Item $key
}

if ($PublicOut) {
    New-Item -ItemType Directory -Force (Split-Path $PublicOut) | Out-Null
    Copy-Item $der $PublicOut -Force
}
"certificate ready in $KeyDir"
