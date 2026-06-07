{ lib
, runCommand
, pkgs
, libva
, nixpkgsRev ? "unknown"
}:

let
  abi = import ./libva-abi.nix {
    inherit lib;
    libvaVersion = libva.version;
  };

  versionOf = package:
    if package == null then
      null
    else if package ? version then
      package.version
    else if package ? name then
      lib.getVersion package.name
    else
      null;

  attrVersion = path:
    versionOf (lib.attrByPath path null pkgs);

  sourceRevOf = package:
    if package == null then
      null
    else if package ? src && builtins.isAttrs package.src && package.src ? rev then
      package.src.rev
    else
      null;

  attrSourceRev = path:
    sourceRevOf (lib.attrByPath path null pkgs);

  firstVersion = paths:
    let
      versions = builtins.filter (version: version != null) (map attrVersion paths);
    in
    if versions == [ ] then null else builtins.head versions;

  firstSourceRev = paths:
    let
      revisions = builtins.filter (rev: rev != null) (map attrSourceRev paths);
    in
    if revisions == [ ] then null else builtins.head revisions;

  report = {
    schemaVersion = 1;
    nixpkgs = {
      rev = nixpkgsRev;
    };
    libva = {
      packageVersion = libva.version;
      initAbi = abi.initAbi;
      initSymbol = abi.initSymbol;
    };
    mesa = {
      version = firstVersion [ [ "mesa" ] ];
    };
    virglrenderer = {
      version = firstVersion [ [ "virglrenderer" ] ];
    };
    crosvm = {
      version = firstVersion [ [ "crosvm" ] ];
      sourceRev = firstSourceRev [ [ "crosvm" ] ];
    };
    cloudHypervisor = {
      version = firstVersion [ [ "cloud-hypervisor" ] ];
      sourceRev = firstSourceRev [ [ "cloud-hypervisor" ] ];
    };
    checks = {
      api = "compile against active libva.dev";
      abi = "export exactly ${abi.initSymbol}";
      behavior = "fake-driver harness under active libva";
    };
  };

  reportJson = builtins.toJSON report + "\n";
in

runCommand "virgl-vaapi-compat-compatibility-report"
  {
    inherit reportJson;
    passAsFile = [ "reportJson" ];
  }
  ''
    mkdir -p "$out/share/virgl-vaapi-compat"
    cp "$reportJsonPath" "$out/share/virgl-vaapi-compat/compatibility-report.json"
    ln -s share/virgl-vaapi-compat/compatibility-report.json "$out/compatibility-report.json"
  ''
