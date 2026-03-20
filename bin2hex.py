def bin_to_hex_txt(bin_file_path, txt_file_path):
    # 1. 读取二进制文件
    with open(bin_file_path, 'rb') as bin_file:
        binary_data = bin_file.read()  # 读取全部内容（小文件适用）

    # 2. 转换为16进制并格式化（每行16字节）
    hex_lines = []
    for i in range(0, len(binary_data), 16):
        # 截取当前行的16字节数据
        chunk = binary_data[i:i+16]
        # 将每个字节转为2位大写16进制，用空格分隔
        hex_line = ' '.join(f'{byte:02X}' for byte in chunk)
        hex_lines.append(hex_line)

    # 3. 写入文本文件
    with open(txt_file_path, 'w') as txt_file:
        txt_file.write('\n'.join(hex_lines))

    print(f"转换完成！\n输入文件: {bin_file_path}\n输出文件: {txt_file_path}")


# ================= 配置区域 =================
# 在这里修改你的输入和输出文件路径
input_bin = "D:\FPGA_prj\XC7A100-LED\XC7A100-LED.runs\impl_1\led.bin"    # 你的.bin文件路径
output_txt = "D:\CubeMX\STM32H743_XC7A100_PS_Configuration\hexoutput.txt"  # 生成的.txt文件路径
# ===========================================

# 执行转换
if __name__ == "__main__":
    bin_to_hex_txt(input_bin, output_txt)