@echo off
for /l %%i in (1,1,10) do (
    echo Running iteration %%i
    vm.exe >> output.txt 2>&1
    echo. >> output.txt
)