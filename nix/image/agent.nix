# kido's guest agent, built from the kido input for darwin/arm64, and the root tree that installs it
# with its launchd job, as kido's bake-image.sh did.
{ lib
, buildGoModule
, runCommand
, kido
}:

let
  agent = buildGoModule {
    pname = "kido-agent";
    version = kido.shortRev;
    src = kido;
    vendorHash = "sha256-Ed2z0/r6kZCz6Ndh3ULYB1M7f0jSwY/jHkbkShfNKso=";
    subPackages = [ "agent" ];
    env.CGO_ENABLED = "0";
    ldflags = [ "-s" "-w" ];
    # buildGoModule takes GOOS and GOARCH from the host's Go
    preBuild = ''
      export GOOS=darwin GOARCH=arm64
    '';
    # darwin test binaries do not run on the build host
    doCheck = false;
    installPhase = ''
      runHook preInstall
      install -Dm755 "$GOPATH/bin/darwin_arm64/agent" "$out/bin/kido-agent"
      runHook postInstall
    '';
    # a Mach-O binary, ad-hoc signed by Go's linker: nothing for the ELF fixups to do, and strip would break the signature
    dontFixup = true;
  };
in
{
  inherit agent;
  root = runCommand "kido-agent-root-${agent.version}" { } ''
    install -Dm755 ${agent}/bin/kido-agent $out/usr/libexec/kido-agent
    mkdir -p $out/System/Library/LaunchDaemons
    cat > $out/System/Library/LaunchDaemons/sh.kido.agent.plist <<'EOF'
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0">
    <dict>
    	<key>Label</key>
    	<string>sh.kido.agent</string>
    	<key>ProgramArguments</key>
    	<array>
    		<string>/usr/libexec/kido-agent</string>
    	</array>
    	<key>RunAtLoad</key>
    	<true/>
    	<key>KeepAlive</key>
    	<true/>
    	<key>StandardOutPath</key>
    	<string>/var/log/kido-agent.log</string>
    	<key>StandardErrorPath</key>
    	<string>/var/log/kido-agent.log</string>
    </dict>
    </plist>
    EOF
    chmod 644 $out/System/Library/LaunchDaemons/sh.kido.agent.plist
  '';
}
