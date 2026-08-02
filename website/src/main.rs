use std::cell::OnceCell;
use std::collections::HashSet;
use std::error::Error;
use std::process::{Command, ExitCode};

#[derive(Default)]
struct Settings {
	// true = include download files and links for all the versions of ted, etc.,
	// false = this is for local documentation, don't include all that.
	for_world_wide_web: bool,
}

fn read_to_string(path: &str) -> Result<String, Box<dyn Error>> {
	Ok(std::fs::read_to_string(path).map_err(|e| format!("Couldn't read {path}: {e}"))?)
}

fn markdown_contents_to_html(contents: &str) -> Result<String, Box<dyn Error>> {
	let options = markdown::Options {
		compile: markdown::CompileOptions {
			allow_dangerous_html: true,
			allow_any_img_src: true,
			..markdown::CompileOptions::default()
		},
		..Default::default()
	};
	Ok(markdown::to_html_with_options(contents, &options).unwrap())
}

fn markdown_to_html(path: &str) -> Result<String, Box<dyn Error>> {
	let contents = read_to_string(path)?;
	markdown_contents_to_html(&contents)
}

fn command_output(cmdline: &[&str]) -> Result<String, Box<dyn Error>> {
	let output = Command::new(cmdline[0])
		.stderr(std::process::Stdio::piped())
		.args(&cmdline[1..])
		.output()?;
	if !output.status.success() {
		Err(format!("{} failed.", cmdline[0]))?;
	}
	Ok(String::from_utf8(output.stdout)?)
}

// path for source tarball for version `version`.
fn source_tarball_path(version: &str) -> String {
	format!("releases/ted-{version}-src.tar.gz")
}

fn package_source(version: &str) -> Result<(), Box<dyn Error>> {
	_ = std::fs::remove_dir_all("tmp-ted");
	let status = Command::new("git")
		.arg("clone")
		.arg("..")
		.arg("--single-branch")
		.arg("--branch")
		.arg(version)
		.arg("tmp-ted")
		.stdout(std::process::Stdio::null())
		// annoyingly the "detached head" thing goes to stderr, so we have to void it.
		.stderr(std::process::Stdio::null())
		.status()
		.map_err(|e| format!("cloning version {version}: {e}"))?;
	if !status.success() {
		Err(format!(
			"cloning version {version}: git clone exited with code {status}"
		))?;
	}
	let files = Command::new("git")
		.current_dir("tmp-ted")
		.arg("ls-files")
		.arg("-z")
		.stderr(std::process::Stdio::piped())
		.output()?;
	if !files.status.success() {
		Err(format!(
			"listing files for {version}: git clone exited with code {status}"
		))?;
	}
	let files = String::from_utf8(files.stdout)?;
	// first package to an intermediate file, in case tar process is interrupted
	let tmp = "tmp.tar.gz";
	let status = Command::new("tar")
		.current_dir("tmp-ted")
		.arg("-czf")
		.arg(tmp)
		.arg("--transform=s,^,ted/,")
		.args(files.split('\0').filter(|x| !x.is_empty()))
		.status()
		.map_err(|e| format!("tarring version {version}: {e}"))?;
	if !status.success() {
		Err(format!(
			"tarring version {version}: tar exited with code {status}"
		))?;
	}
	std::fs::rename(format!("tmp-ted/{tmp}"), source_tarball_path(version))?;
	Ok(())
}

fn process_changelog(settings: &Settings) -> Result<String, Box<dyn Error>> {
	let changelog_in = read_to_string("../CHANGELOG.md")?;
	let versions = changelog_in.split("## ");
	let mut changelog_out = String::new();
	let mut release_files = vec![];
	if settings.for_world_wide_web && !std::fs::exists("releases")? {
		Err("Building with --www, but releases/ doesn't exist.
It should exist and have all the old ted installers.")?;
	}
	if settings.for_world_wide_web {
		for f in std::fs::read_dir("releases").map_err(|e| format!("reading releases/: {e}"))? {
			let f = f?;
			if let Some(s) = f.file_name().to_str() {
				release_files.push(s.to_owned());
			}
		}
	}
	for description in versions {
		if description.trim().is_empty() {
			continue;
		}
		changelog_out.push_str("## ");
		changelog_out.push_str(description);
		let version = description
			.split(' ')
			.next()
			.ok_or("Couldn't extract version number")?;
		if !settings.for_world_wide_web
			|| version.starts_with("0.")
			|| version.starts_with("1.")
			|| version == "2.0"
			|| version == "2.1"
			|| version == "2.2"
			|| version == "2.2r1"
			|| version == "2.3"
			|| version == "2.8.4"
		{
			// no tag available
			continue;
		}
		if !std::fs::exists(source_tarball_path(version))? {
			println!("Cloning {version}...");
			package_source(version)?;
		}
		let mut debs = vec![];
		let mut msis = vec![];
		let mut static_sdl_debs = vec![];
		for file in &release_files {
			if file.ends_with("_amd64.deb") && file.starts_with(&format!("ted_{version}-")) {
				debs.push(file);
			}
			if file.ends_with("_amd64.deb")
				&& file.starts_with(&format!("ted-static-sdl_{version}-"))
			{
				static_sdl_debs.push(file);
			}
			if file == &format!("ted_{version}_amd64.msi") {
				msis.push(file);
			}
		}
		if debs.len() > 1 {
			Err(format!(
				"More than one deb file found for version {version}"
			))?;
		}
		if static_sdl_debs.len() > 1 {
			Err(format!(
				"More than one static-sdl deb file found for version {version}"
			))?;
		}
		if msis.len() > 1 {
			Err(format!(
				"More than one msi file found for version {version}"
			))?;
		}
		if let [deb] = &debs[..] {
			changelog_out.push_str(&format!(
				"- [Debian/Ubuntu x86-64 (.deb)](releases/{deb})\n"
			));
		}
		if let [deb_static_sdl] = &static_sdl_debs[..] {
			changelog_out.push_str(&format!(
				"- [Debian/Ubuntu x86-64, static SDL (.deb)](releases/{deb_static_sdl})\n"
			));
		}
		if let [msi] = &msis[..] {
			changelog_out.push_str(&format!("- [Windows x86-64 (.msi)](releases/{msi})\n"));
		}
		changelog_out.push_str(&format!(
			"- [Source code (.tar.gz)]({})\n",
			source_tarball_path(version)
		));
		changelog_out.push('\n');
	}
	Ok(changelog_out)
}

fn try_main(settings: &Settings) -> Result<(), Box<dyn Error>> {
	_ = std::fs::remove_dir_all("dist");
	std::fs::create_dir("dist")?;

	let version: String = command_output(&["../version.sh"])?.trim_ascii_end().into();
	let version_parts: Vec<_> = version.split('.').collect();
	assert_eq!(version_parts.len(), 3, "ted version should have 3 parts");
	for part in version_parts {
		assert!(
			part.parse::<u32>().is_ok(),
			"ted version parts should all be numbers"
		);
	}
	let readme = markdown_to_html("../README.md")?;
	let guide = markdown_to_html("../GUIDE.md")?;
	let changelog = markdown_contents_to_html(&process_changelog(settings)?)?;
	// The CSS is small enough that it's probably better just to include it inline
	let style_template = read_to_string("main.css")?;
	let colors = [
		// (color name, dark, light, extradark)
		("BG", "#001", "#eee", "#000"),
		("TEXT", "#fff", "#000", "#fff"),
		("BORDER", "#a77", "#844", "#9a7"),
		("SELECTED_TAB_BG", "#714f4f", "#ccadad", "#66714f"),
		("LINK", "#a7f", "#70a", "#c0f"),
	];
	let mut style_dark = style_template.clone();
	let mut style_light = style_template.clone();
	let mut style_extradark = style_template.clone();
	// could just use light-dark() or var(), but that isn't supported
	// on older browsers such as IE.
	for (name, dark, light, extradark) in colors {
		let name = format!("$COLOR_{name}");
		style_dark = style_dark.replace(&name, dark);
		style_extradark = style_extradark.replace(&name, extradark);
		style_light = style_light.replace(&name, light);
	}
	let style = format!(
		r#"<style id="style-dark">{style_dark}</style>
<script id="style-light" type="text/plain">{style_light}</script>
<script id="style-extradark" type="text/plain">{style_extradark}</script>"#
	);
	let mut readme_index = String::new();
	{
		let mut outputting = false;
		for line in readme.split('\n') {
			match line {
				"<!-- WEBSITE INDEX.HTML ON -->" => outputting = true,
				"<!-- WEBSITE INDEX.HTML OFF -->" => outputting = false,
				_ => {
					if outputting {
						readme_index.push_str(line);
						readme_index.push('\n');
					}
				}
			}
		}
	}
	let nav_template = read_to_string("template-nav.html")?;
	let color_scheme_selector = format!(
		"<script>{}</script>",
		read_to_string("color-scheme-selector.js")?
	);
	let index_www_only: OnceCell<String> = Default::default();
	let process_html_file = |filename: &str, source: &str| -> Result<String, Box<dyn Error>> {
		let mut nav = nav_template.replace(
			&format!("<td><a href=\"{filename}\""),
			&format!("<td data-selected><a href=\"{filename}\""),
		);
		nav.push_str(&color_scheme_selector);
		Ok(source
			.replace("${GUIDE}", &guide)
			.replace("${VERSION}", &version)
			.replace("${NAV}", &nav)
			.replace("${STYLE}", &style)
			.replace("${README}", &readme_index)
			.replace("${CHANGELOG}", &changelog)
			.replace("${INDEX_WWW_ONLY}", index_www_only.get().map_or("", |s| s)))
	};
	// Download links, etc. should only be shown on index.html if we actually have
	// release files.
	if settings.for_world_wide_web {
		index_www_only
			.set(process_html_file(
				"",
				&read_to_string("template-index-www-only.html")?,
			)?)
			.unwrap();
	}
	let files = command_output(&["git", "ls-files", "-z"])?;
	let files = files.split('\0');
	let mut excluded: HashSet<&'static str> = [
		".",
		"..",
		"dist",
		"main.css",
		"Cargo.lock",
		"Cargo.toml",
		"rustfmt.toml",
		"color-scheme-selector.js",
		"publish.sh",
	]
	.into();
	if !settings.for_world_wide_web {
		excluded.extend([
			"ted.png",
			"install-repo.sh",
			"pommicket.gpg",
			"pommicket.sources",
		]);
	}
	for filename in files {
		if filename.is_empty() {
			continue;
		}
		if filename.starts_with("template-")
			|| filename.starts_with(".")
			|| filename.starts_with("src/")
			|| excluded.contains(filename)
		{
			continue;
		}
		let output_path: String = format!("dist/{filename}");
		if filename.ends_with(".html") {
			println!("Processing HTML {filename}");
			let source = read_to_string(filename)?;
			std::fs::write(&output_path, process_html_file(filename, &source)?)?;
		} else {
			println!("Copying {filename}");
			std::fs::copy(filename, &output_path)?;
		}
	}
	println!("Copying favicon.ico");
	std::fs::copy("../assets/icon.ico", "dist/favicon.ico")?;
	if settings.for_world_wide_web {
		std::fs::create_dir_all("dist/releases")?;
		println!("Link release files...");
		for file in std::fs::read_dir("releases")? {
			let file = file?.file_name();
			let file = file
				.to_str()
				.ok_or_else(|| format!("Invalid UTF-8 in filename: {}", file.to_string_lossy()))?;
			if !(file.ends_with(".tar.gz") || file.ends_with(".msi") || file.ends_with(".deb")) {
				continue;
			}
			std::fs::hard_link(format!("releases/{file}"), format!("dist/releases/{file}"))?;
		}
	}
	println!("All done!");
	Ok(())
}

fn main() -> ExitCode {
	let mut args: Vec<String> = std::env::args().collect();
	args.remove(0);
	let mut settings = Settings::default();
	if let Some(i) = args.iter().position(|x| x == "--www") {
		settings.for_world_wide_web = true;
		args.remove(i);
	}
	if !args.is_empty() {
		eprintln!("Unrecognized arguments: {args:?}");
		return ExitCode::FAILURE;
	}
	match try_main(&settings) {
		Ok(()) => ExitCode::SUCCESS,
		Err(e) => {
			eprintln!("{e}");
			ExitCode::FAILURE
		}
	}
}
