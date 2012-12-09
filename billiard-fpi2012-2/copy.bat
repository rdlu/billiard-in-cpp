for %%I in (*.png *.obj *.dll *.vertex *.fragment *.jpg) do xcopy /y %%I ..\Debug\
for %%I in (*.png *.obj *.dll *.vertex *.fragment *.jpg) do xcopy /y %%I ..\Release\
pause