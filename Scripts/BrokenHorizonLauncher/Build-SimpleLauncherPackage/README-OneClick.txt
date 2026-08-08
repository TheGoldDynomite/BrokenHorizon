To ship a simple player launcher:
1) Configure values in BHLauncherConfig.json:
   - Repo (owner/repo)
   - AssetFilter (or set ReleaseTagFallback)
   - DirectReleaseUrl for a direct asset URL.
2) Distribute this folder as a zip. 
3) User double-clicks Launch-BrokenHorizon-OneClick.bat.
4) If you are seeing GitHub 404, confirm the release is actually published at:
   https://github.com/<owner>/<repo>/releases
5) For tagged updates like https://github.com/<owner>/<repo>/releases/tag/Update:
   - set ReleaseTagFallback to "Update"
   - keep SkipGitHub=false
   - set ForceUpdateFromTag=true if you re-upload under same tag.
6) If your release URL is a direct zip asset, put it in DirectReleaseUrl and set SkipGitHub=true.

7) The packaged launcher reads BHLauncherConfig.json from its folder, so rebuild/repack after any config changes.
8) If GitHub API keeps returning 404, set DirectReleaseUrl to:
   https://github.com/TheGoldDynomite/BrokenHorizon/releases/tag/Update
   and set SkipGitHub=true. This bypasses release API calls.
   With AllowExecutableFallback=false, only zip links from that tag page are used.
