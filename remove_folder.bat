DEL /F/Q/S %1\*.* > NUL
If %ERRORLEVEL% NEQ 0	ECHO DELETE %1 FAILED
RMDIR /Q/S %1
If %ERRORLEVEL% NEQ 0	ECHO RMDIR %1 FAILED


