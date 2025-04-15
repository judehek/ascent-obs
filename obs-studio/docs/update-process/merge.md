## Update the OBS origin branch

- after finsihing [get the code](), checkout branch 'obs-origin' localy.
- delete all folder content exclude .git folder!
- copy the obs-origin files from [get the code](), (without the buid64 and build32 folders)
- add all files to git (also ignore files! like '.dll') 
- delete from git all deleted files
- do not forget to add 'additional_install_files' folder and files
- commit and push (name it with the obs version number like 27.2.3)
- run cmake and verify the branch is build and run.

## Merge Process
- craete new branch from lastes (main?) branch
- than merge from origin/obs-origin branch to current
- fix confilcts,
- test, run, commit and push!


