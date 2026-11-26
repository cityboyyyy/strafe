param([string]$Dll, [string]$Inf, [string]$OutDir, [string]$KeyDir = (Join-Path $env:USERPROFILE ".controller-signing"))

$ErrorActionPreference = "Continue"
$kit = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0"
$stampinf = Join-Path $kit "x64\stampinf.exe"
$inf2cat = Join-Path $kit "x86\Inf2Cat.exe"
$signtool = Join-Path $kit "x64\signtool.exe"

New-Item -ItemType Directory -Force $OutDir | Out-Null
Get-ChildItem $OutDir -Exclude "DuoController.dll" | Remove-Item -Force -Recurse
if ((Resolve-Path $Dll).Path -ne (Join-Path (Resolve-Path $OutDir).Path "DuoController.dll")) {
    Copy-Item $Dll (Join-Path $OutDir "DuoController.dll") -Force
}
$stagedInf = Join-Path $OutDir "DuoController.inf"
Copy-Item $Inf $stagedInf -Force

& $stampinf -f $stagedInf -d "*" -a "amd64" -u "2.25.0" -v "1.0.0.0" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "stampinf failed" }

& $inf2cat "/driver:$OutDir" "/os:10_X64" "/uselocaltime" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Inf2Cat failed" }

$pfx = Join-Path $KeyDir "driver_signing.pfx"
$passFile = Join-Path $KeyDir "pfx_password.txt"
if (-not (Test-Path $pfx)) {
    "driver package ready in $OutDir (unsigned: no signing certificate found)"
    exit 0
}
$password = Get-Content $passFile -Raw
foreach ($file in @("DuoController.dll", "DuoController.cat")) {
    & $signtool sign /q /fd SHA256 /f $pfx /p $password (Join-Path $OutDir $file) | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "signtool failed on $file" }
}
"driver package ready in $OutDir"
