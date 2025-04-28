@echo off
setlocal

:: Chemin vers les DLLs GTK3 (à adapter selon votre installation MSYS2)
set GTK_PATH=C:\msys64\mingw64\bin

:: Copier les DLLs nécessaires
copy "%GTK_PATH%\libgtk-3-0.dll" .
copy "%GTK_PATH%\libgdk-3-0.dll" .
copy "%GTK_PATH%\libgobject-2.0-0.dll" .
copy "%GTK_PATH%\libglib-2.0-0.dll" .
copy "%GTK_PATH%\libcairo-2.dll" .
copy "%GTK_PATH%\libpango-1.0-0.dll" .
copy "%GTK_PATH%\libpangocairo-1.0-0.dll" .
copy "%GTK_PATH%\libgdk_pixbuf-2.0-0.dll" .
copy "%GTK_PATH%\libatk-1.0-0.dll" .
copy "%GTK_PATH%\libepoxy-0.dll" .
copy "%GTK_PATH%\libffi-8.dll" .
copy "%GTK_PATH%\libgio-2.0-0.dll" .
copy "%GTK_PATH%\libgmodule-2.0-0.dll" .
copy "%GTK_PATH%\libharfbuzz-0.dll" .
copy "%GTK_PATH%\libintl-8.dll" .
copy "%GTK_PATH%\libpangoft2-1.0-0.dll" .
copy "%GTK_PATH%\libpangowin32-1.0-0.dll" .
copy "%GTK_PATH%\libpcre-1.dll" .
copy "%GTK_PATH%\libpixman-1-0.dll" .
copy "%GTK_PATH%\libpng16-16.dll" .
copy "%GTK_PATH%\libz.dll" .

:: Copy .env file to build directory
copy "..\.env" .

:: Lancer l'application
.\gtk_app.exe

endlocal 