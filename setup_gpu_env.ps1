param(
    [switch]$Recreate
)

$ErrorActionPreference = "Stop"

function Get-PythonLauncher {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        return "py"
    }
    if (Get-Command python -ErrorAction SilentlyContinue) {
        return "python"
    }
    throw "Python launcher not found. Install Python and ensure py or python is on PATH."
}

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

$venvPath = Join-Path $repoRoot ".venv_gpu"
$pythonLauncher = Get-PythonLauncher

if ($Recreate -and (Test-Path $venvPath)) {
    Remove-Item -Recurse -Force $venvPath
}

if (!(Test-Path $venvPath)) {
    if ($pythonLauncher -eq "py") {
        & py -3 -m venv $venvPath
    } else {
        & python -m venv $venvPath
    }
}

$venvPython = Join-Path $venvPath "Scripts\python.exe"
if (!(Test-Path $venvPython)) {
    throw "Failed to create .venv_gpu. Python executable not found in venv."
}

Write-Host "[1/5] Upgrade pip/setuptools/wheel"
& $venvPython -m pip install --upgrade pip setuptools wheel

Write-Host "[2/5] Install common training dependencies"
& $venvPython -m pip install -r requirements-train.txt

Write-Host "[3/5] Install CUDA PyTorch (cu128)"
& $venvPython -m pip install -r requirements-gpu-cu128.txt

Write-Host "[4/5] Verify torch + CUDA"
& $venvPython -c "import torch; print('torch', torch.__version__); print('cuda_available', torch.cuda.is_available()); print('cuda_device_count', torch.cuda.device_count()); print('cuda_name', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'N/A')"

Write-Host "[5/5] Quick trainer syntax check"
& $venvPython -m py_compile train_som_gpu.py

Write-Host "Done. .venv_gpu is ready."
