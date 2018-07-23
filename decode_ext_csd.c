Decode EXT_CSD of eMMC 5.0 device
Message ID 	1391812643-20389-1-git-send-email-gwendal@chromium.org
State 	New, archived
Headers 	show
Commit Message
Gwendal Grignou Feb. 7, 2014, 10:37 p.m. UTC

Display new attributes in Extended CSD register introduced by eMMC 5.0:
'mmc  extcsd read /dev/mmcblk0' returns for eMMC 5.0 device:

Comments
Grant Grundler Feb. 25, 2014, 10:26 p.m. UTC | #1

+Ulf

Hi Chris,
Any chance of this getting reviewed or accepted soon?

thanks!
grant

On Fri, Feb 7, 2014 at 2:37 PM, Gwendal Grignou <gwendal@chromium.org> wrote:
> Display new attributes in Extended CSD register introduced by eMMC 5.0:
> 'mmc  extcsd read /dev/mmcblk0' returns for eMMC 5.0 device:
>
> =============================================
>   Extended CSD rev 1.7 (MMC 5.0)
> =============================================
>
> Card Supported Command sets [S_CMD_SET: 0x01]
> ...
> Extended partition attribute support [EXT_SUPPORT: 0x03]
> Supported modes [SUPPORTED_MODES: 0x01]
> FFU features [FFU_FEATURES: 0x00]
> Operation codes timeout [OPERATION_CODE_TIMEOUT: 0x00]
> FFU Argument [FFU_ARG: 0x00000000]
> Number of FW sectors correctly programmed [NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED: 0]
> Vendor proprietary health report:
> [VENDOR_PROPRIETARY_HEALTH_REPORT[301]]: 0x00
> ...
> [VENDOR_PROPRIETARY_HEALTH_REPORT[270]]: 0x00
> Device life time estimation type B [DEVICE_LIFE_TIME_EST_TYP_B: 0x01]
>  i.e. 0% - 10% device life time used
> Device life time estimation type B [DEVICE_LIFE_TIME_EST_TYP_A: 0x01]
>  i.e. 0% - 10% device life time used
> Pre EOL information [PRE_EOL_INFO: 0x01]
>  i.e. Normal
> Optimal read size [OPTIMAL_READ_SIZE: 0x00]
> Optimal write size [OPTIMAL_WRITE_SIZE: 0x10]
> Optimal trim unit size [OPTIMAL_TRIM_UNIT_SIZE: 0x01]
> Device version [DEVICE_VERSION: 0x00 - 0x00]
> Firmware version:
> [FIRMWARE_VERSION[261]]: 0x00
> ...
> [FIRMWARE_VERSION[254]]: 0x05
> Power class for 200MHz, DDR at VCC= 3.6V [PWR_CL_DDR_200_360: 0x00]
> Generic CMD6 Timer [GENERIC_CMD6_TIME: 0x0a]
> Power off notification [POWER_OFF_LONG_TIME: 0x3c]
> Cache Size [CACHE_SIZE] is 65536 KiB
> ...
>
> Signed-off-by: Gwendal Grignou <gwendal@chromium.org>
> Reviewed-by: Grant Grundler <grundler@chromium.org>
> ---
>  mmc_cmds.c | 146 ++++++++++++++++++++++++++++++++++++++++++-------------------
>  1 file changed, 102 insertions(+), 44 deletions(-)
>
> diff --git a/mmc_cmds.c b/mmc_cmds.c
> index b8afa74..4b9b12e 100644
> --- a/mmc_cmds.c
> +++ b/mmc_cmds.c
> @@ -424,12 +424,17 @@ int do_status_get(int nargs, char **argv)
>         return ret;
>  }
>
> +__u32 get_word_from_ext_csd(__u8 *ext_csd_loc)
> +{
> +       return (ext_csd_loc[3] << 24) |
> +               (ext_csd_loc[2] << 16) |
> +               (ext_csd_loc[1] << 8)  |
> +               ext_csd_loc[0];
> +}
> +
>  unsigned int get_sector_count(__u8 *ext_csd)
>  {
> -       return (ext_csd[EXT_CSD_SEC_COUNT_3] << 24) |
> -       (ext_csd[EXT_CSD_SEC_COUNT_2] << 16) |
> -       (ext_csd[EXT_CSD_SEC_COUNT_1] << 8)  |
> -       ext_csd[EXT_CSD_SEC_COUNT_0];
> +       return get_word_from_ext_csd(&ext_csd[EXT_CSD_SEC_COUNT_0]);
>  }
>
>  int is_blockaddresed(__u8 *ext_csd)
> @@ -701,6 +706,23 @@ int do_read_extcsd(int nargs, char **argv)
>         int fd, ret;
>         char *device;
>         const char *str;
> +       const char *ver_str[] = {
> +               "4.0",  /* 0 */
> +               "4.1",  /* 1 */
> +               "4.2",  /* 2 */
> +               "4.3",  /* 3 */
> +               "Obsolete", /* 4 */
> +               "4.41", /* 5 */
> +               "4.5",  /* 6 */
> +               "5.0",  /* 7 */
> +       };
> +       int boot_access;
> +       const char* boot_access_str[] = {
> +               "No access to boot partition",          /* 0 */
> +               "R/W Boot Partition 1",                 /* 1 */
> +               "R/W Boot Partition 2",                 /* 2 */
> +               "R/W Replay Protected Memory Block (RPMB)", /* 3 */
> +       };
>
>         CHECK(nargs != 2, "Usage: mmc extcsd read </path/to/mmcblkX>\n",
>                           exit(1));
> @@ -721,28 +743,12 @@ int do_read_extcsd(int nargs, char **argv)
>
>         ext_csd_rev = ext_csd[192];
>
> -       switch (ext_csd_rev) {
> -       case 6:
> -               str = "4.5";
> -               break;
> -       case 5:
> -               str = "4.41";
> -               break;
> -       case 3:
> -               str = "4.3";
> -               break;
> -       case 2:
> -               str = "4.2";
> -               break;
> -       case 1:
> -               str = "4.1";
> -               break;
> -       case 0:
> -               str = "4.0";
> -               break;
> -       default:
> +       if ((ext_csd_rev < sizeof(ver_str)/sizeof(char*)) &&
> +           (ext_csd_rev != 4))
> +               str = ver_str[ext_csd_rev];
> +       else
>                 goto out_free;
> -       }
> +
>         printf("=============================================\n");
>         printf("  Extended CSD rev 1.%d (MMC %s)\n", ext_csd_rev, str);
>         printf("=============================================\n\n");
> @@ -789,13 +795,77 @@ int do_read_extcsd(int nargs, char **argv)
>                         ext_csd[495]);
>                 printf("Extended partition attribute support"
>                         " [EXT_SUPPORT: 0x%02x]\n", ext_csd[494]);
> +       }
> +       if (ext_csd_rev >= 7) {
> +               int j;
> +               int eol_info;
> +               char* eol_info_str[] = {
> +                       "Not Defined",  /* 0 */
> +                       "Normal",       /* 1 */
> +                       "Warning",      /* 2 */
> +                       "Urgent",       /* 3 */
> +               };
> +
> +               printf("Supported modes [SUPPORTED_MODES: 0x%02x]\n",
> +                       ext_csd[493]);
> +               printf("FFU features [FFU_FEATURES: 0x%02x]\n",
> +                       ext_csd[492]);
> +               printf("Operation codes timeout"
> +                       " [OPERATION_CODE_TIMEOUT: 0x%02x]\n",
> +                       ext_csd[491]);
> +               printf("FFU Argument [FFU_ARG: 0x%08x]\n",
> +                       get_word_from_ext_csd(&ext_csd[487]));
> +               printf("Number of FW sectors correctly programmed"
> +                       " [NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED: %d]\n",
> +                       get_word_from_ext_csd(&ext_csd[302]));
> +               printf("Vendor proprietary health report:\n");
> +               for (j = 301; j >= 270; j--)
> +                       printf("[VENDOR_PROPRIETARY_HEALTH_REPORT[%d]]:"
> +                               " 0x%02x\n", j, ext_csd[j]);
> +               for (j = 269; j >= 268; j--) {
> +                       __u8 life_used=ext_csd[j];
> +                       printf("Device life time estimation type B"
> +                               " [DEVICE_LIFE_TIME_EST_TYP_%c: 0x%02x]\n",
> +                               'B' + (j - 269), life_used);
> +                       if (life_used >= 0x1 && life_used <= 0xa)
> +                               printf(" i.e. %d%% - %d%% device life time"
> +                                       " used\n",
> +                                       (life_used - 1) * 10, life_used * 10);
> +                       else if (life_used == 0xb)
> +                               printf(" i.e. Exceeded its maximum estimated"
> +                                       " device life time\n");
> +               }
> +               eol_info = ext_csd[267];
> +               printf("Pre EOL information [PRE_EOL_INFO: 0x%02x]\n",
> +                       eol_info);
> +               if (eol_info < sizeof(eol_info_str)/sizeof(char*))
> +                       printf(" i.e. %s\n", eol_info_str[eol_info]);
> +               else
> +                       printf(" i.e. Reserved\n");
> +
> +               printf("Optimal read size [OPTIMAL_READ_SIZE: 0x%02x]\n",
> +                       ext_csd[266]);
> +               printf("Optimal write size [OPTIMAL_WRITE_SIZE: 0x%02x]\n",
> +                       ext_csd[265]);
> +               printf("Optimal trim unit size"
> +                       " [OPTIMAL_TRIM_UNIT_SIZE: 0x%02x]\n", ext_csd[264]);
> +               printf("Device version [DEVICE_VERSION: 0x%02x - 0x%02x]\n",
> +                       ext_csd[263], ext_csd[262]);
> +               printf("Firmware version:\n");
> +               for (j = 261; j >= 254; j--)
> +                       printf("[FIRMWARE_VERSION[%d]]:"
> +                               " 0x%02x\n", j, ext_csd[j]);
> +
> +               printf("Power class for 200MHz, DDR at VCC= 3.6V"
> +                       " [PWR_CL_DDR_200_360: 0x%02x]\n", ext_csd[253]);
> +       }
> +       if (ext_csd_rev >= 6) {
>                 printf("Generic CMD6 Timer [GENERIC_CMD6_TIME: 0x%02x]\n",
>                         ext_csd[248]);
>                 printf("Power off notification [POWER_OFF_LONG_TIME: 0x%02x]\n",
>                         ext_csd[247]);
>                 printf("Cache Size [CACHE_SIZE] is %d KiB\n",
> -                       ext_csd[249] << 0 | (ext_csd[250] << 8) |
> -                       (ext_csd[251] << 16) | (ext_csd[252] << 24));
> +                       get_word_from_ext_csd(&ext_csd[249]));
>         }
>
>         /* A441: Reserved [501:247]
> @@ -945,24 +1015,12 @@ int do_read_extcsd(int nargs, char **argv)
>                 printf(" User Area Enabled for boot\n");
>                 break;
>         }
> -       switch (reg & EXT_CSD_BOOT_CFG_ACC) {
> -       case 0x0:
> -               printf(" No access to boot partition\n");
> -               break;
> -       case 0x1:
> -               printf(" R/W Boot Partition 1\n");
> -               break;
> -       case 0x2:
> -               printf(" R/W Boot Partition 2\n");
> -               break;
> -       case 0x3:
> -               printf(" R/W Replay Protected Memory Block (RPMB)\n");
> -               break;
> -       default:
> +       boot_access = reg & EXT_CSD_BOOT_CFG_ACC;
> +       if (boot_access < sizeof(boot_access_str) / sizeof(char*))
> +               printf(" %s\n", boot_access_str[boot_access]);
> +       else
>                 printf(" Access to General Purpose partition %d\n",
> -                       (reg & EXT_CSD_BOOT_CFG_ACC) - 3);
> -               break;
> -       }
> +                       boot_access - 3);
>
>         printf("Boot config protection [BOOT_CONFIG_PROT: 0x%02x]\n",
>                 ext_csd[178]);
> --
> 1.9.0.rc1.175.g0b1dcb5
>
--
To unsubscribe from this list: send the line "unsubscribe linux-mmc" in
the body of a message to majordomo@vger.kernel.org
More majordomo info at  http://vger.kernel.org/majordomo-info.html

Patch

=============================================
  Extended CSD rev 1.7 (MMC 5.0)
=============================================

Card Supported Command sets [S_CMD_SET: 0x01]
...
Extended partition attribute support [EXT_SUPPORT: 0x03]
Supported modes [SUPPORTED_MODES: 0x01]
FFU features [FFU_FEATURES: 0x00]
Operation codes timeout [OPERATION_CODE_TIMEOUT: 0x00]
FFU Argument [FFU_ARG: 0x00000000]
Number of FW sectors correctly programmed [NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED: 0]
Vendor proprietary health report:
[VENDOR_PROPRIETARY_HEALTH_REPORT[301]]: 0x00
...
[VENDOR_PROPRIETARY_HEALTH_REPORT[270]]: 0x00
Device life time estimation type B [DEVICE_LIFE_TIME_EST_TYP_B: 0x01]
 i.e. 0% - 10% device life time used
Device life time estimation type B [DEVICE_LIFE_TIME_EST_TYP_A: 0x01]
 i.e. 0% - 10% device life time used
Pre EOL information [PRE_EOL_INFO: 0x01]
 i.e. Normal
Optimal read size [OPTIMAL_READ_SIZE: 0x00]
Optimal write size [OPTIMAL_WRITE_SIZE: 0x10]
Optimal trim unit size [OPTIMAL_TRIM_UNIT_SIZE: 0x01]
Device version [DEVICE_VERSION: 0x00 - 0x00]
Firmware version:
[FIRMWARE_VERSION[261]]: 0x00
...
[FIRMWARE_VERSION[254]]: 0x05
Power class for 200MHz, DDR at VCC= 3.6V [PWR_CL_DDR_200_360: 0x00]
Generic CMD6 Timer [GENERIC_CMD6_TIME: 0x0a]
Power off notification [POWER_OFF_LONG_TIME: 0x3c]
Cache Size [CACHE_SIZE] is 65536 KiB
...

Signed-off-by: Gwendal Grignou <gwendal@chromium.org>
Reviewed-by: Grant Grundler <grundler@chromium.org>
---
 mmc_cmds.c | 146 ++++++++++++++++++++++++++++++++++++++++++-------------------
 1 file changed, 102 insertions(+), 44 deletions(-)

diff --git a/mmc_cmds.c b/mmc_cmds.c
index b8afa74..4b9b12e 100644
--- a/mmc_cmds.c
+++ b/mmc_cmds.c
@@ -424,12 +424,17 @@  int do_status_get(int nargs, char **argv)
 	return ret;
 }
 
+__u32 get_word_from_ext_csd(__u8 *ext_csd_loc)
+{
+ext_csd_locreturn (ext_csd_loc[3] << 24) |
+ext_csd_loc(ext_csd_loc[2] << 16) |
+ext_csd_loc(ext_csd_loc[1] << 8)  |
+16ext_csd_loc[0];
+}
+
 unsigned int get_sector_count(__u8 *ext_csd)
 {
-ext_csdreturn (ext_csd[EXT_CSD_SEC_COUNT_3] << 24) |
-24(ext_csd[EXT_CSD_SEC_COUNT_2] << 16) |
-16(ext_csd[EXT_CSD_SEC_COUNT_1] << 8)  |
-EXT_CSD_SEC_COUNT_1ext_csd[EXT_CSD_SEC_COUNT_0];
+EXT_CSD_SEC_COUNT_0return get_word_from_ext_csd(&ext_csd[EXT_CSD_SEC_COUNT_0]);
 }
 
 int is_blockaddresed(__u8 *ext_csd)
@@ -701,6 +706,23 @@  int do_read_extcsd(int nargs, char **argv)
 	int fd, ret;
 	char *device;
 	const char *str;
+strconst char *ver_str[] = {
+char"4.0",char/* 0 */
+ver_str"4.1",ver_str/* 1 */
+char"4.2",char/* 2 */
+ver_str"4.3",ver_str/* 3 */
+char"Obsolete", /* 4 */
+char"4.41",41/* 5 */
+char"4.5",  /* 6 */
+41"5.0",  /* 7 */
+41};
+41int boot_access;
+boot_accessconst char* boot_access_str[] = {
+char"No access to boot partition",boot/* 0 */
+partition"R/W Boot Partition 1",partition/* 1 */
+Partition"R/W Boot Partition 2",partition/* 2 */
+Partition"R/W Replay Protected Memory Block (RPMB)", /* 3 */
+RPMB};
 
 	CHECK(nargs != 2, "Usage: mmc extcsd read </path/to/mmcblkX>\n",
 			  exit(1));
@@ -721,28 +743,12 @@  int do_read_extcsd(int nargs, char **argv)
 
 	ext_csd_rev = ext_csd[192];
 
-192switch (ext_csd_rev) {
-ext_csd_revcase 6:
-ext_csd_revstr = "4.5";
-ext_csd_revcasebreak;
-ext_csd_revcasebreakcase 5:
-ext_csd_revcasebreakstr = "4.41";
-ext_csd_revcasebreakstrbreak;
-ext_csd_revcasebreakstrbreakcase 3:
-ext_csd_revcasebreakstrbreakstr = "4.3";
-ext_csd_revcasebreakstrbreakcasebreak;
-ext_csd_revcasebreakstrbreakcasebreakcase 2:
-ext_csd_revcasebreakstrbreakcasebreakstr = "4.2";
-ext_csd_revcasebreakstrbreakcasebreakcasebreak;
-ext_csd_revcasebreakstrbreakcasebreakcasebreakcase 1:
-ext_csd_revcasebreakstrbreakcasebreakcasebreakstr = "4.1";
-ext_csd_revcasebreakstrbreakcasebreakcasebreakcasebreak;
-ext_csd_revcasebreakstrbreakcasebreakcasebreakcasebreakcase 0:
-ext_csd_revcasebreakstrbreakcasebreakcasebreakcasebreakstr = "4.0";
-ext_csd_revcasebreakstrbreakcasebreakcasebreakcasebreakcasebreak;
-ext_csd_revcasebreakstrbreakcasebreakcasebreakcasebreakcasebreakdefault:
+ext_csd_revcasebreakstrbreakcasebreakcasebreakcasebreakcasebreakdefaultif ((ext_csd_rev < sizeof(ver_str)/sizeof(char*)) &&
+char    (ext_csd_rev != 4))
+charstr = ver_str[ext_csd_rev];
+ext_csd_revelse
 		goto out_free;
-out_free}
+
 	printf("=============================================\n");
 	printf("  Extended CSD rev 1.%d (MMC %s)\n", ext_csd_rev, str);
 	printf("=============================================\n\n");
@@ -789,13 +795,77 @@  int do_read_extcsd(int nargs, char **argv)
 			ext_csd[495]);
 		printf("Extended partition attribute support"
 			" [EXT_SUPPORT: 0x%02x]\n", ext_csd[494]);
+494}
+494if (ext_csd_rev >= 7) {
+494ifint j;
+ext_csd_revint eol_info;
+ext_csd_revintchar* eol_info_str[] = {
+eol_info"Not Defined",Defined/* 0 */
+eol_info"Normal",Normal/* 1 */
+Defined"Warning",Warning/* 2 */
+Normal"Urgent",Urgent/* 3 */
+Normal};
+
+Urgentprintf("Supported modes [SUPPORTED_MODES: 0x%02x]\n",
+SUPPORTED_MODESext_csd[493]);
+SUPPORTED_MODESext_csdprintf("FFU features [FFU_FEATURES: 0x%02x]\n",
+FFU_FEATURESext_csd[492]);
+FFU_FEATURESext_csdprintf("Operation codes timeout"
+Operation" [OPERATION_CODE_TIMEOUT: 0x%02x]\n",
+OPERATION_CODE_TIMEOUText_csd[491]);
+OPERATION_CODE_TIMEOUText_csdprintf("FFU Argument [FFU_ARG: 0x%08x]\n",
+FFU_ARGget_word_from_ext_csd(&ext_csd[487]));
+ext_csdprintf("Number of FW sectors correctly programmed"
+sectors" [NUMBER_OF_FW_SECTORS_CORRECTLY_PROGRAMMED: %d]\n",
+programmedget_word_from_ext_csd(&ext_csd[302]));
+ext_csdprintf("Vendor proprietary health report:\n");
+healthfor (j = 301; j >= 270; j--)
+healthforprintf("[VENDOR_PROPRIETARY_HEALTH_REPORT[%d]]:"
+301" 0x%02x\n", j, ext_csd[j]);
+02xfor (j = 269; j >= 268; j--) {
+02xfor__u8 life_used=ext_csd[j];
+02xfor__u8printf("Device life time estimation type B"
+life" [DEVICE_LIFE_TIME_EST_TYP_%c: 0x%02x]\n",
+life'B' + (j - 269), life_used);
+lifeif (life_used >= 0x1 && life_used <= 0xa)
+lifeifprintf(" i.e. %d%% - %d%% device life time"
+0xa" used\n",
+device(life_used - 1) * 10, life_used * 10);
+deviceelse if (life_used == 0xb)
+deviceelseprintf(" i.e. Exceeded its maximum estimated"
+deviceelseprintf" device life time\n");
+life}
+timeeol_info = ext_csd[267];
+ext_csdprintf("Pre EOL information [PRE_EOL_INFO: 0x%02x]\n",
+PRE_EOL_INFOeol_info);
+02xif (eol_info < sizeof(eol_info_str)/sizeof(char*))
+eol_info_strprintf(" i.e. %s\n", eol_info_str[eol_info]);
+eol_info_strelse
+eol_info_strprintf(" i.e. Reserved\n");
+
+eol_info_strprintfprintf("Optimal read size [OPTIMAL_READ_SIZE: 0x%02x]\n",
+OPTIMAL_READ_SIZEext_csd[266]);
+OPTIMAL_READ_SIZEext_csdprintf("Optimal write size [OPTIMAL_WRITE_SIZE: 0x%02x]\n",
+OPTIMAL_WRITE_SIZEext_csd[265]);
+OPTIMAL_WRITE_SIZEext_csdprintf("Optimal trim unit size"
+trim" [OPTIMAL_TRIM_UNIT_SIZE: 0x%02x]\n", ext_csd[264]);
+ext_csdprintf("Device version [DEVICE_VERSION: 0x%02x - 0x%02x]\n",
+DEVICE_VERSIONext_csd[263], ext_csd[262]);
+ext_csdprintf("Firmware version:\n");
+Firmwarefor (j = 261; j >= 254; j--)
+Firmwareforprintf("[FIRMWARE_VERSION[%d]]:"
+261" 0x%02x\n", j, ext_csd[j]);
+
+02xprintf("Power class for 200MHz, DDR at VCC= 3.6V"
+at" [PWR_CL_DDR_200_360: 0x%02x]\n", ext_csd[253]);
+253}
+253if (ext_csd_rev >= 6) {
 		printf("Generic CMD6 Timer [GENERIC_CMD6_TIME: 0x%02x]\n",
 			ext_csd[248]);
 		printf("Power off notification [POWER_OFF_LONG_TIME: 0x%02x]\n",
 			ext_csd[247]);
 		printf("Cache Size [CACHE_SIZE] is %d KiB\n",
-CACHE_SIZEext_csd[249] << 0 | (ext_csd[250] << 8) |
-249(ext_csd[251] << 16) | (ext_csd[252] << 24));
+ext_csdget_word_from_ext_csd(&ext_csd[249]));
 	}
 
 	/* A441: Reserved [501:247]
@@ -945,24 +1015,12 @@  int do_read_extcsd(int nargs, char **argv)
 		printf(" User Area Enabled for boot\n");
 		break;
 	}
-breakswitch (reg & EXT_CSD_BOOT_CFG_ACC) {
-EXT_CSD_BOOT_CFG_ACCcase 0x0:
-EXT_CSD_BOOT_CFG_ACCcaseprintf(" No access to boot partition\n");
-bootbreak;
-bootbreakcase 0x1:
-bootbreakcaseprintf(" R/W Boot Partition 1\n");
-Bootbreak;
-Bootbreakcase 0x2:
-Bootbreakcaseprintf(" R/W Boot Partition 2\n");
-Bootbreak;
-Bootbreakcase 0x3:
-Bootbreakcaseprintf(" R/W Replay Protected Memory Block (RPMB)\n");
-Blockbreak;
-Blockbreakdefault:
+Blockbreakdefaultboot_access = reg & EXT_CSD_BOOT_CFG_ACC;
+EXT_CSD_BOOT_CFG_ACCif (boot_access < sizeof(boot_access_str) / sizeof(char*))
+sizeofprintf(" %s\n", boot_access_str[boot_access]);
+boot_accesselse
 		printf(" Access to General Purpose partition %d\n",
-General(reg & EXT_CSD_BOOT_CFG_ACC) - 3);
-regbreak;
-regbreak}
+regboot_access - 3);
 
 	printf("Boot config protection [BOOT_CONFIG_PROT: 0x%02x]\n",
 		ext_csd[178]);


