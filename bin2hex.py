from pathlib import Path


def _binary_file_to_hex(input_file_path, out_file_path, target_byte=323163, out_format="txt", file_label="Bin"):
    """
    Convert a binary-style file (.bin/.bit) to a hex txt/csv file and mark bytes around target_byte.
    """
    input_path = Path(input_file_path)
    output_path = Path(out_file_path)

    with input_path.open("rb") as input_file:
        binary_data = input_file.read()

    total_bytes = len(binary_data)
    print(f"{file_label} file total bytes: {total_bytes}")

    if target_byte < 0 or target_byte >= total_bytes:
        print(
            f"Warning: target_byte={target_byte} is out of range. "
            f"File size is only {total_bytes} bytes."
        )
        return

    hex_bytes = [f"{byte:02X}" for byte in binary_data]

    start_idx = max(0, target_byte - 8)
    end_idx = min(total_bytes - 1, target_byte + 8)
    target_data = binary_data[start_idx:end_idx + 1]

    target_hex = []
    for idx_in_target, byte_pos in enumerate(range(start_idx, end_idx + 1)):
        byte_hex = f"{target_data[idx_in_target]:02X}"
        if byte_pos == target_byte:
            target_hex.append(f"[{byte_hex}]")
        else:
            target_hex.append(byte_hex)
    target_hex_str = " ".join(target_hex)

    if out_format.lower() == "csv":
        hex_lines = [",".join(hex_bytes[i:i + 16]) for i in range(0, total_bytes, 16)]
        with output_path.open("w", encoding="utf-8", newline="") as csv_file:
            csv_file.write("\n".join(hex_lines))
    else:
        hex_lines = [" ".join(hex_bytes[i:i + 16]) for i in range(0, total_bytes, 16)]
        with output_path.open("w", encoding="utf-8") as txt_file:
            txt_file.write("\n".join(hex_lines))
            txt_file.write("\n\n" + "=" * 80 + "\n")
            txt_file.write(f"Target byte: {target_byte} (0-based index)\n")
            txt_file.write(f"Byte range: [{start_idx} ~ {end_idx}]\n")
            txt_file.write(f"Hex data: {target_hex_str}\n")

    print(
        f"Conversion done.\n"
        f"Input file: {input_path}\n"
        f"Output file: {output_path}\n"
        f"Format: {out_format}"
    )
    print(f"Target region data: {target_hex_str}")


def bin_to_hex(bin_file_path, out_file_path, target_byte=323163, out_format="txt"):
    _binary_file_to_hex(bin_file_path, out_file_path, target_byte, out_format, file_label="Bin")


def bit_to_hex(bit_file_path, out_file_path, target_byte=323163, out_format="txt"):
    _binary_file_to_hex(bit_file_path, out_file_path, target_byte, out_format, file_label="Bit")


# ================= Configuration =================
convert_mode = "bit"  # "bin" or "bit"

input_bin = r"D:\FPGA_prj\XC7A100-LED\XC7A100-LED.runs\impl_1\led.bin"
input_bit = r"D:\FPGA_prj\XC7A100-LED\XC7A100-LED.runs\impl_1\led.bit"

output_bin_file = r"D:\CubeMX\STM32H743_XC7A100_PS_Configuration\hexoutput_bin.txt"
output_bit_file = r"D:\CubeMX\STM32H743_XC7A100_PS_Configuration\hexoutput_bit.txt"

out_format = "txt"  # "txt" or "csv"
target_byte = 95909
# ================================================


if __name__ == "__main__":
    if convert_mode.lower() == "bit":
        bit_to_hex(input_bit, output_bit_file, target_byte, out_format)
    else:
        bin_to_hex(input_bin, output_bin_file, target_byte, out_format)
