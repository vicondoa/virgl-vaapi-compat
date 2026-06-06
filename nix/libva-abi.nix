{ lib
, libvaVersion ? null
, pkgConfigVersion ? null
}:

let
  rawVersion =
    if pkgConfigVersion != null then
      pkgConfigVersion
    else if libvaVersion != null then
      libvaVersion
    else
      throw "libva ABI derivation requires libvaVersion or pkgConfigVersion";

  versionMatch = builtins.match "([0-9]+)\\.([0-9]+)(\\..*)?" rawVersion;

  toInt = value:
    if builtins.match "[0-9]+" value == null then
      throw "libva version component '${value}' is not numeric"
    else
      builtins.fromJSON value;

  versionMajor =
    if versionMatch == null then
      throw "Could not derive libva init ABI from version '${rawVersion}'"
    else
      toInt (builtins.elemAt versionMatch 0);

  versionMinor = toInt (builtins.elemAt versionMatch 1);

  supportedVersion =
    versionMajor == 1 || versionMajor == 2;

  source =
    if pkgConfigVersion != null || versionMajor == 1 then
      "pkg-config"
    else
      "package";

  initMajor = 1;
  initMinor = versionMinor;
in

if !supportedVersion then
  throw ''
    Unsupported libva major version '${toString versionMajor}' from '${rawVersion}'.
    Expected libva package version 2.N or pkg-config-compatible VA-API version 1.N.
  ''
else
{
  inherit source rawVersion versionMajor versionMinor initMajor initMinor;

  initAbi = "${toString initMajor}_${toString initMinor}";
  initSymbol = "__vaDriverInit_${toString initMajor}_${toString initMinor}";
  expectedPkgConfigMajor = initMajor;
  expectedPkgConfigVersionPrefix = "${toString initMajor}.${toString initMinor}.";
}
