@echo off
cd /d %~dps0

@echo %~1
@echo %~dps1

set env_build_script=make_win32_build_environment.sh
set codeblocks_toolchain_path=%~dps1

if NOT Exist %codeblocks_toolchain_path%bin\WINenvVER (
	@echo "unable to find %codeblocks_toolchain_path%bin\WINenvVER"
	exit 0
)

for /f "tokens=*" %%a in ('type %codeblocks_toolchain_path%bin\WINenvVER') do (
	set ver=%%a
)

if NOT Exist %env_build_script% (
	@echo "unable to find %~dps0%env_build_script%"
	exit 0
)

for /f "tokens=2 delims== " %%a in ('type %env_build_script% ^| findstr /I "^environment_ver"') do (
	set CB=%%a
)

if %ver%==%CB% (
	@echo Win32 build environment is up2date
	exit 0
)
if %ver% GEQ %CB% (
	@echo Win32 build environment is a testbuild
	exit 0
)


@echo "Win32 build environment is outdated"
@echo update your build environment

echo %~dps0 | findstr /I "%codeblocks_toolchain_path%\" 2>&1>NULL && (
	set this=%~dps0
	call set this=%%this:%codeblocks_toolchain_path%=%%
	call set this=%%this:\=/%%
	start %codeblocks_toolchain_path%msys.bat
)

if NOT "%this%"=="" (
	@echo .
	@echo enter this line into mingw32
	@echo /%this%%env_build_script% create
	@echo .
	@echo you will find the new environment at %codeblocks_toolchain_path%home\%USERDOMAIN%
	start explorer %codeblocks_toolchain_path%home\%USERDOMAIN%
	@echo you will find the new environment compressed at %codeblocks_toolchain_path%home\%USERDOMAIN%
	@echo and uncompressed at %tmp%
)

exit 0
