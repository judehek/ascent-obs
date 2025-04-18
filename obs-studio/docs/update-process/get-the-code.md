2. follow obs-sudio build instructions (windows)
   ~~https://github.com/obsproject/obs-studio/wiki/Install-Instructions#windows-build-directions~~
     https://github.com/obsproject/obs-studio/wiki/build-instructions-for-windows (29.X.X)
4. ~~addtional instructions (cmake gui):~~
   ~~- add OBS_VERSION_OVERRIDE (for example 27.2.3)~~
   ~~- add both QTDIR32, QTDIR64 and DepsPath32,DepsPath64~~
   ~~- check COMPILE_D3D12_HOOK~~
   ~~- unchek BUILD_BROWSER~~
   - since obs 29.X.X building OBS is much easier: 
   - open PwoerShell at root obs folder and run  'CI/build-windows.ps1'  -CombinedArchs
   - to set dedicate version (for build):
      1. open ..cmake\Modules\VersionConfig.cmake 
      2. add:
set(OBS_VERSION_OVERRIDE 
    "29.0.2.0"
    CACHE STRING "OBS Version")

5. Folder strcutre:
  ```
 /src/
     /obs-studio
     /ascent-obs
     /obs-build-dependencies
```


