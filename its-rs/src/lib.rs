use num_bigint::BigUint;
use num_traits::{One, Zero};
use std::io::{BufRead, BufReader};

fn dut1_field_blank(line: &str) -> bool { 
    // this checks DUT1 field is blank
    if line.len() < 69 { return true; }
    line[58..69].chars().all(|c| c.is_whitespace())
}

pub fn load_finals(finals: &[u8]) -> Result<(Vec<f64>, Vec<f64>), String> {
    // this returns mjd and dut1 from finals.all
    let reader = BufReader::new(finals);
    let mut mjd_vec = Vec::with_capacity(20000);
    let mut dut1_vec = Vec::with_capacity(20000);

    for line in reader.lines() {
        let line = line.map_err(|e| e.to_string())?;
        if line.len() < 69 { continue; }
        if line.starts_with("MJD") { continue; }
        if dut1_field_blank(&line) { continue; }

        let mjd = line[7..15].trim().parse::<f64>().unwrap_or(0.0);
        let dut1 = line[58..69].trim().parse::<f64>().unwrap_or(0.0);
        if mjd > 0.0 && dut1 > -10.0 && dut1 < 10.0 {
            mjd_vec.push(mjd);
            dut1_vec.push(dut1);
        }
    }

    Ok((mjd_vec, dut1_vec))    
}

#[cfg(test)]
mod tests {
    use super::*;
    fn tests(){
        todo!("😏😏😏😏😏MEWING🤫🧏 ♂️");
    }
}
