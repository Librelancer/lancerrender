$SDL2_URI = "https://www.libsdl.org/release/SDL2-devel-2.0.14-VC.zip"

$PSScriptRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
$PackagesFolder = Join-Path $PSScriptRoot packages
New-Item -ItemType Directory -Force -Path $PackagesFolder

if ($PSVersionTable.PSEdition -ne 'Core') {
    # Attempt to set highest encryption available for SecurityProtocol.
    # PowerShell will not set this by default (until maybe .NET 4.6.x). This
    # will typically produce a message for PowerShell v2 (just an info
    # message though)
    try {
        # Set TLS 1.2 (3072), then TLS 1.1 (768), then TLS 1.0 (192), finally SSL 3.0 (48)
        # Use integers because the enumeration values for TLS 1.2 and TLS 1.1 won't
        # exist in .NET 4.0, even though they are addressable if .NET 4.5+ is
        # installed (.NET 4.5 is an in-place upgrade).
        [System.Net.ServicePointManager]::SecurityProtocol = 3072 -bor 768 -bor 192 -bor 48
      } catch {
        Write-Output 'Unable to set PowerShell to use TLS 1.2 and TLS 1.1 due to old .NET Framework installed. If you see underlying connection closed or trust errors, you may need to upgrade to .NET Framework 4.5+ and PowerShell v3'
      }
}

$ZipPath = Join-Path $PackagesFolder sdl2.zip

(New-Object System.Net.WebClient).DownloadFile($SDL2_URI, $ZipPath);

#Extract Zip File
function Expand-ZIPFile($file, $destination)
{
	if ($PSVersionTable.PSEdition -ne 'Core') {
		$shell = new-object -com shell.application
		$zip = $shell.NameSpace($file)
		foreach($item in $zip.items())
		{
			$shell.Namespace($destination).copyhere($item)
		}
	} else {
		Expand-Archive -Path $file -DestinationPath $destination
	}
}

Expand-ZIPFile $ZipPath $PackagesFolder
Copy-Item -Force (Join-Path $PSScriptRoot "sdl2-config.cmake.in") (Join-Path $PackagesFolder "SDL2-2.0.14/sdl2-config.cmake")

