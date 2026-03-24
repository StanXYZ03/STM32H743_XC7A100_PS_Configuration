def bin_to_hex(bin_file_path, out_file_path, target_byte=323163, out_format='txt'):
    """
    将bin文件转换为16进制txt/csv文件（UTF-8编码），并在末尾标注目标字节前后8字节数据
    :param bin_file_path: 输入bin文件路径
    :param out_file_path: 输出文件路径
    :param target_byte: 要定位的字节位置（索引从0开始）
    :param out_format: 输出格式 'txt' 或 'csv'
    """
    # 1. 读取二进制文件（一次性读取，小文件适用；大文件可改成分块）
    with open(bin_file_path, 'rb') as bin_file:
        binary_data = bin_file.read()
    total_bytes = len(binary_data)
    print(f"Bin文件总字节数: {total_bytes}")

    # 2. 校验目标字节是否在文件范围内
    if target_byte < 0 or target_byte >= total_bytes:
        print(f"警告：目标字节{target_byte}超出文件范围（文件仅{total_bytes}字节）")
        return

    # 3. 转换全部数据为16进制
    hex_bytes = [f'{byte:02X}' for byte in binary_data]

    # 4. 提取目标字节前后8字节数据（处理边界情况）
    start_idx = max(0, target_byte - 8)
    end_idx = min(total_bytes - 1, target_byte + 8)
    target_data = binary_data[start_idx:end_idx+1]
    
    # 转换为16进制并标记目标字节
    target_hex = []
    for idx_in_target, byte_pos in enumerate(range(start_idx, end_idx+1)):
        byte_hex = f'{target_data[idx_in_target]:02X}'
        if byte_pos == target_byte:
            target_hex.append(f'【{byte_hex}】')  # 标记目标字节
        else:
            target_hex.append(byte_hex)
    target_hex_str = ' '.join(target_hex)

    # 5. 输出文件（txt或csv）
    if out_format.lower() == 'csv':
        with open(out_file_path, 'w', encoding='utf-8', newline='') as csv_file:
            csv_file.write('hex_byte\n')
            for h in hex_bytes:
                csv_file.write(h + '\n')
    else:
        # 默认txt：每行16字节
        hex_lines = [' '.join(hex_bytes[i:i+16]) for i in range(0, total_bytes, 16)]
        with open(out_file_path, 'w', encoding='utf-8') as txt_file:
            txt_file.write('\n'.join(hex_lines))
            txt_file.write('\n\n' + '='*80 + '\n')
            txt_file.write(f'定位区域：第{target_byte}字节（索引从0开始）前后8字节数据\n')
            txt_file.write(f'字节范围：[{start_idx} ~ {end_idx}]\n')
            txt_file.write(f'16进制数据：{target_hex_str}\n')

    print(f"转换完成！\n输入文件: {bin_file_path}\n输出文件: {out_file_path}\n格式: {out_format}")
    print(f"目标区域数据：{target_hex_str}")


# ================= 配置区域 =================
# 在这里修改你的输入和输出文件路径
input_bin = "D:\FPGA_prj\XC7A100-LED\XC7A100-LED.runs\impl_1\led.bin"    # 你的.bin文件路径
output_file = "D:\CubeMX\STM32H743_XC7A100_PS_Configuration\hexoutput.csv"  # 生成的输出文件路径
# out_format 可选 'txt' 或 'csv'
out_format = 'csv'

target_byte = 95909       # 要定位的字节位置（可修改）
# ===========================================

# 执行转换
if __name__ == "__main__":
    bin_to_hex(input_bin, output_file, target_byte, out_format)
