/*
testing comments
/* /* /* /* /* hello */ */ */ */ */
yay
*/
use std::fs::File;
use std::io::{Result, BufRead, BufReader};
fn main() -> Result<()> {
	let file = File::open("test.rs")?;
	let mut reader = BufReader::new(file);
	loop {
		let mut line = String::new();
		if reader.read_line(&mut line)? == 0 {
			// reached end of file
			break;
		}
		print!("{}", line);
	}
	print!("
	string
	");
	Ok(())
}
