#!/usr/bin/env python
# Automatically generated input script for dfu_file_generator.py

dfu_image_parameters = {
    "gen_flash_image": "True",
    "bank": "bank1"
}

flash_device_parameters = {
    "block_size": 65536,
    "boot_block_size": None,
    "alt_image_offset": 2097152
}

host_tools_parameters = {
    "devkit": r"E:\qtil\ADK_QCC512x_QCC302x_WIN_6.3.0.154",
    "NvsCmd": r"E:\qtil\ADK_QCC512x_QCC302x_WIN_6.3.0.154\tools\bin\nvscmd.exe",
    "SecurityCmd": r"E:\qtil\ADK_QCC512x_QCC302x_WIN_6.3.0.154\tools\bin\SecurityCmd.exe",
    "UpgradeFileGen": r"E:\qtil\ADK_QCC512x_QCC302x_WIN_6.3.0.154\tools\bin\UpgradeFileGen.exe",
    "crescendo_upd_config": r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\scripts\20190611192811.upd",
    "dfu_dir": r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\output",
    "folder_for_rsa_files": r"F:/Fang_WorkNotes/project/W1/SW/W1_V005_ADK630/apps/applications/earbud/qcc512x_qcc302x/QCC3020-AA_DEV-BRD-R2-AA/dfu",
}

flash0 = {
    "flash_device": flash_device_parameters,
    "dfu_image": dfu_image_parameters,
    "host_tools": host_tools_parameters,
    "chip_type": "QCC512X",
    "encrypt": True,
    "hardware_encrypted": False,
    "encryption_file": r"F:/Fang_WorkNotes/project/W1/SW/W1_V005_ADK630/apps/applications/earbud/qcc512x_qcc302x/QCC3020-AA_DEV-BRD-R2-AA/efuse_key.txt",
    "signing_mode": "all",
    "layout": [
        ("curator_fs", {
           "src_file" : r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\input\curator_config_filesystem.xuv",
            "src_file_signed": False,
            "authenticate": True,
            "capacity": 1024,
            }),
        ("apps_p0", {
           "src_file" : r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\input\apps_p0_firmware.xuv",
            "src_file_signed": True,
            "authenticate": True,
            "capacity": 589824,
            }),
        ("apps_p1", {
           "src_file" : r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\input\earbud.xuv",
            "authenticate": True,
            "capacity": 458752,
            }),
        ("device_ro_fs", {
            "authenticate": True,
            "capacity": 4096,
            "inline_auth_hash": True,
            }),
        ("rw_config", {
            "capacity": 131072,
            }),
        ("rw_fs", {
            "capacity": 131072,
            }),
        ("ro_cfg_fs", {
           "src_file" : r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\input\firmware_config_filesystem_dfu.xuv",
            "authenticate": True,
            "capacity": 65536,
            }),
        ("ro_fs", {
           "src_file" : r"F:\Fang_WorkNotes\project\W1\SW\W1_V005_ADK630\apps\applications\earbud\qcc512x_qcc302x\QCC3020-AA_DEV-BRD-R2-AA\dfu\20190611192811\input\customer_ro_filesystem_dfu.xuv",
            "authenticate": True,
            "capacity": 458752,
            }),
    ]
}

flash1 = {
    "flash_device": flash_device_parameters,
    "layout": []
}
