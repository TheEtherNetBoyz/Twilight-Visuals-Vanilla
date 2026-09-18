@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
if errorlevel 1 exit /b %errorlevel%
"C:\Users\TheEtherNetBoyz\AppData\Local\Programs\Python\Python314\Scripts\ninja.exe" -C "C:\Users\TheEtherNetBoyz\Downloads\tempmfb\Twilight Visuals Vanilla\build-latest"
exit /b %errorlevel%
