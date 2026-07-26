use std::collections::HashSet;
use std::error::Error;
use std::process::{Command, ExitCode};

fn read_to_string(path: &str) -> Result<String, Box<dyn Error>> {
	Ok(std::fs::read_to_string(path).map_err(|e| format!("Couldn't read {path}: {e}"))?)
}

fn markdown_to_html(path: &str) -> Result<String, Box<dyn Error>> {
	let options = markdown::Options {
		compile: markdown::CompileOptions {
			allow_dangerous_html: true,
			allow_any_img_src: true,
			..markdown::CompileOptions::default()
		},
		..Default::default()
	};
	let contents = read_to_string(path)?;
	Ok(markdown::to_html_with_options(&contents, &options).unwrap())
}

fn command_output(cmdline: &[&str]) -> Result<String, Box<dyn Error>> {
	let output = Command::new(cmdline[0]).args(&cmdline[1..]).output()?;
	if !output.status.success() {
		Err(format!(
			"{} failed.\nstderr: {}",
			cmdline[0],
			String::from_utf8_lossy(&output.stderr)
		))?;
	}
	Ok(String::from_utf8(output.stdout)?)
}

fn try_main() -> Result<(), Box<dyn Error>> {
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
	// The CSS is small enough that it's probably better just to include it inline
	let style = format!("<style>{}</style>", read_to_string("main.css")?);
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
	let process_html_file = |path: &str| -> String {
		path.replace("${GUIDE}", &guide)
			.replace("${VERSION}", &version)
			.replace("${NAV}", &nav_template)
			.replace("${STYLE}", &style)
			.replace("${README}", &readme_index)
	};
	let files = command_output(&["git", "ls-files", "-z"])?;
	let files = files.split('\0');
	let excluded: HashSet<&'static str> = [
		".",
		"..",
		"dist",
		"main.css",
		"Cargo.lock",
		"Cargo.toml",
		"rustfmt.toml",
		"src",
	]
	.into();
	for filename in files {
		if filename.is_empty() {
			continue;
		}
		if filename.starts_with("template-")
			|| filename.starts_with(".")
			|| excluded.contains(filename)
		{
			continue;
		}
		let output_path: String = format!("dist/{filename}");
		if filename.ends_with(".html") {
			println!("Processing HTML {filename}");
			let source = read_to_string(filename)?;
			std::fs::write(&output_path, process_html_file(&source))?;
		} else {
			println!("Copying {filename}");
			std::fs::copy(filename, &output_path)?;
		}
	}
	Ok(())
}

fn main() -> ExitCode {
	match try_main() {
		Ok(()) => ExitCode::SUCCESS,
		Err(e) => {
			eprintln!("{e}");
			ExitCode::FAILURE
		}
	}
}
