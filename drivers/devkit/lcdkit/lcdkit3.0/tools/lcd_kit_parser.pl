#!/usr/bin/perl -w

#get the input target platform name
my $target_plat_name = shift;
my $target_file_type = shift;
my $LCD_KIT_TOOLPATH = "../../../../../chipset_common/devkit/lcdkit/lcdkit3.0/tools";
#Execution start time
my $parser_start_time = time;

my $version_sub_path = "product_config/devkit_config";

if ($target_plat_name eq ""){
    print("The target platform name doesn't input!\n");
    exit 1;
}
#debug_print("The target plat you input is: $target_plat_name \n");

my @platform_chip_map = (
            [ "msm8952_64",             "qcom",          "msm8952"    ],
            [ "msm8953_64",             "qcom",          "msm8953"    ],
            [ "msm8937_64",             "qcom",          "msm8937"    ],
            [ "msm8937_32",             "qcom",          "msm8937"    ],
            [ "hi6250",                 "hisi",          "hi6250"     ],
            [ "hi3660",                 "hisi",          "hi3660"     ],
            [ "kirin970",               "hisi",          "kirin970"   ],
            [ "kirin980",               "hisi",          "kirin980"   ],
            [ "kirin990",               "hisi",          "kirin990"   ],
            [ "k39tv1_64_bsp",          "mtk",           "k39tv1_64_bsp"],
            [ "k61v1_64_mexico",        "mtk",           "k61v1_64_mexico"],
            [ "k62v1_64_mexico",        "mtk",           "k62v1_64_mexico"]);

#get plat name by parse chip name, so the chip name must be 'hixxxx' or 'msmxxxx'
my $chip_plat = '';
my $chip_name = '';
for (my $count = 0; $count < @platform_chip_map; $count++) {
    if ($target_plat_name eq $platform_chip_map[$count][0]) {
        #debug_print("The target plat you input match list: $count \n");
        $chip_name = $platform_chip_map[$count][2];
        $chip_plat = $platform_chip_map[$count][1];
    }
}

if (($chip_plat ne "qcom") && ($chip_plat ne "hisi") && ($chip_plat ne "mtk")){
    print("The chip plat: $chip_plat you input is error!\n");
    exit 1;
}

if ($chip_plat eq 'qcom')
{
    require "./lcdkit-base-lib.pl";
}
else
{
    require "$LCD_KIT_TOOLPATH/lcd_kit_base_lib.pl";
}

debug_print("get parser parameter is: $chip_plat $chip_name $target_file_type\n");

my $parse_error_string = get_err_string();
debug_print("get error string is : $parse_error_string!\n");

if (($target_file_type ne "dtsi") && ($target_file_type ne "trebledto") && ($target_file_type ne "head")
 && ($target_file_type ne "dto-head") && ($target_file_type ne "effect") && ($target_file_type ne "all"))
{
    error_print("The target file type doesn't input or input is error!\n");
    exit 1;
}
debug_print("The target file type you input is: $target_file_type \n");

my $out_head_file_path = "";
my $out_effect_file_path = "";
my $target_dtsi_file_path = "";

if (($target_file_type eq "dtsi") || ($target_file_type eq "trebledto") || ($target_file_type eq "all"))
{
    $target_dtsi_file_path = shift;
    debug_print("get dtsi file path: $target_dtsi_file_path\n");
}

if (($target_file_type eq "head") || ($target_file_type eq "dto-head") || ($target_file_type eq "all"))
{
    $out_head_file_path = shift;
    debug_print("get head file path: $out_head_file_path\n");
}

if (($target_file_type eq "effect") || ($target_file_type eq "all"))
{
    $out_effect_file_path = shift;
    debug_print("get effect file path: $out_effect_file_path\n");
}

# get the abs path for tools folder, the tool will execute in this folder
my $working_path = File::Spec->rel2abs($0);
debug_print("working in path $working_path\n");
$working_path = dirname($working_path);
debug_print("working in path $working_path\n");
chdir $working_path;

my $plat_path;
my $root_path = "$working_path/../../../../../../..";
if ($chip_plat eq "qcom")
{
    $plat_path = "$root_path/vendor/huawei/chipset_common/config/qcom/$chip_name/$target_plat_name/devkit/lcdkit3.0/default";
}
if ($chip_plat eq "mtk")
{
    $plat_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/config/mtk/$chip_name/lcdkit";
}
if ($chip_plat eq "hisi")
{
    $plat_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/config/hisi/$chip_name/lcdkit";
}

debug_print("get chip plat xml file path is: $plat_path\n");

if (!-e "$plat_path"){
    error_print("chip platform default xml file path not exist!\n");
    exit 1;
}


#collect plat xmls
my $plat_file;
my @plat_xmls = glob("$plat_path/*.xml");
foreach $plat_file (@plat_xmls) {
    debug_print("get plat xml files $plat_file\n");
}

#collect lcd xmls
my $panel_xml_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/panel";
my @panel_xmls = glob("$panel_xml_path/*.xml");
foreach my $panel_file (@panel_xmls) {
    debug_print("get panel xml files $panel_file\n");
}

my $out_dtsi_file_path;
my $vender_list_path;
my $local_perl_path = "$root_path/vendor/huawei/extra/kernel/drivers/lcd/tools";
my $devkit_tool_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/tools";

#clean lcd generate files
if (($target_file_type eq "dtsi") || ($target_file_type eq "trebledto") || ($target_file_type eq "all"))
{
    $out_dtsi_file_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/panel/dtsi";
    if (!-e "$out_dtsi_file_path"){
        debug_print("out dtsi file path not exist, create it now!\n");
        system("mkdir " . $out_dtsi_file_path);
    }
    
    my @file_temp = glob("$out_dtsi_file_path/*.dtsi");
    for (@file_temp) {    unlink $_;    }

    if ($target_dtsi_file_path eq "")
    {
        if ($chip_plat eq "hisi")
        {
            $target_dtsi_file_path = "$root_path/kernel/linux-4.1/arch/arm64/boot/dts/auto-generate/$chip_name/lcdkit3.0";
        }
        else
        {
            $target_dtsi_file_path = "$root_path/kernel/msm-3.18/arch/arm64/boot/dts/qcom/lcdkit3.0";
        }
    }
    
    if (-e "$target_dtsi_file_path"){
        debug_print("target dtsi file path exist, delete and create it now!\n");
        system("rm -f -r " . $target_dtsi_file_path);
    }
    system("mkdir -p " . $target_dtsi_file_path);
}

if (($target_file_type eq "head") || ($target_file_type eq "dto-head") || ($target_file_type eq "all"))
{
    if ($out_head_file_path eq "")
    {
        $out_head_file_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/panel/head";
    }
  
    if (!-e "$out_head_file_path"){
        debug_print("out head file path not exist, create it now!\n");
        system("mkdir -p " . $out_head_file_path);
    }
    
    @file_temp = glob("$out_head_file_path/*.h");
    for (@file_temp) {
        #unlink $_;
    }
}

if ((($target_file_type eq "effect") || ($target_file_type eq "all")) && ($chip_plat eq "hisi"))
{
    if ($out_effect_file_path eq "")
    {
        $out_effect_file_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/panel/effect";
    }
    if (!-e "$out_effect_file_path"){
        debug_print("out effect file path not exist, create it now!\n");
        system("mkdir -p " . $out_effect_file_path);
    }
    
    @file_temp = glob("$out_effect_file_path/*.h");
    for (@file_temp) {    unlink $_;    }
}

if ($chip_plat eq "qcom")
{
    $vender_list_path
        = "$root_path/vendor/huawei/chipset_common/config/qcom/$chip_name/$target_plat_name/devkit/lcdkit3.0";
}

if ($chip_plat eq "mtk")
{
    $vender_list_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/config/mtk/$chip_name";
}

if ($chip_plat eq "hisi")
{
    $vender_list_path = "$root_path/vendor/huawei/chipset_common/devkit/lcdkit/lcdkit3.0/config/hisi/$chip_name";
}

#get lcd list
my @lcd_map_group;
my @lcd_effect_map_group;
my @lcd_default_panel_map_group;
my @lcd_product_map_group;

my @lcd_map_list = get_module_map_list("platform", \@plat_xmls, '/hwlcd/lcdlist/lcd');
if (@lcd_map_list eq 0){
    error_print("cann't get lcd map list!\n");
    exit 1;
}

my @product_names;
my @lcd_file_names;
#hisi effect add
my $lcd_xml_parser = new XML::LibXML;
foreach my $list_entry (@lcd_map_list) {
    debug_print("get lcd map entry: $list_entry\n");

    $list_entry =~ s/ //g;
    $list_entry =~ s/\t//g;
    $list_entry =~ s/\n//g;
    my @lcd_element = split /,/, $list_entry;

    my $lcd_name    = $lcd_element[0];
    my $lcd_gpio    = $lcd_element[1];
    my $lcd_bdid    = $lcd_element[2];
    my $product     = $lcd_element[3];
    my $vender_fd   = $lcd_element[4];
    my $version_fd  = $lcd_element[5];

    debug_print("parsing: $lcd_name $lcd_gpio $lcd_bdid $product $vender_fd $version_fd\n");

    #get product list
    my $product_num = 0;
    $product =~ s/-/_/g;
    $product = lc($product);
    foreach my $product_name (@product_names) {
        debug_print("$product_name : $product \n");
        if ($product_name eq $product) {
            $product_num++;
        }
    }
    
    if ($product_num eq 0) {
        debug_print("push : $product \n");
        push(@product_names, $product);
    }

    my @parse_xml_files;

    if ($vender_fd ne 'def')
    {
        #collect version xmls
        if ($version_fd ne 'def')
        {
            my @version_xmls;
            if ($chip_plat eq "qcom")
            {
                @version_xmls = glob("$vender_list_path/$vender_fd/$version_fd/*.xml");
            }
            else
            {
                @version_xmls = glob("$vender_list_path/$vender_fd/$version_fd/*.xml");
            }
            
            foreach my $version_file (@version_xmls) {
                debug_print("get version xml files $version_file\n");
            }
            my $version_xml = get_xml_file(\@version_xmls, $lcd_name . '.xml');
            if ($version_xml ne $parse_error_string) {
                push(@parse_xml_files, $version_xml);
            }
            else
            {
                error_print("get version $lcd_name xml file failed!\n");
            }
        }
                
        #collect vender xmls    
        my @vender_xmls;
        if ($chip_plat eq "qcom")
        {
            @vender_xmls = glob("$vender_list_path/$vender_fd/default/*.xml");
        }
        else
        {
            @vender_xmls = glob("$vender_list_path/$vender_fd/*.xml");
        }   
        
        foreach my $vender_file (@vender_xmls) {
            debug_print("get vender xml files $vender_file\n");
        }

        my $vender_xml = get_xml_file(\@vender_xmls, $lcd_name . '.xml');
        if ($vender_xml ne $parse_error_string) {
            push(@parse_xml_files, $vender_xml);
        }
        else
        {
            error_print("get product $lcd_name xml file failed!\n");
        }
    }
    
    my $plat_xml = get_xml_file(\@plat_xmls, '\w+' . 'platform.xml');
    if ($plat_xml ne $parse_error_string) {
        push(@parse_xml_files, $plat_xml);
    }
    else
    {
        error_print("get platform $lcd_name xml file failed!\n");
    }
    
    my $panel_xml = get_xml_file(\@panel_xmls, $lcd_name . '.xml');
    my $xmldoc = $lcd_xml_parser->parse_file($panel_xml);
    my $compatible = "";
    for my $property($xmldoc->findnodes('/hwlcd/PanelEntry/PanelCompatible')) {
                $compatible = $property->textContent();
    }
    if ($vender_fd ne 'def')
    {
        my @vender_xmls;
        if ($chip_plat eq "qcom")
        {
            @vender_xmls = glob("$vender_list_path/$vender_fd/default/*.xml");
        }
        else
        {
            @vender_xmls = glob("$vender_list_path/$vender_fd/*.xml");
        }
        my $vender_xml = get_xml_file(\@vender_xmls, $lcd_name . '.xml');
        $xmldoc = $lcd_xml_parser->parse_file($vender_xml);
        for my $property($xmldoc->findnodes('/hwlcd/PanelEntry/PanelCompatible')) {
            $compatible = $property->textContent();
        }
    }
    if ($panel_xml ne $parse_error_string) {
        push(@parse_xml_files, $panel_xml);
    }
    else
    {
        error_print("get panel $lcd_name xml file failed!\n");
    }

    if (@parse_xml_files eq 0) {
        error_print("get lcd xml files fail!\n");
        exit 1;
    }

    $product =~ s/-/_/g;
    $lcd_name =~ s/-/_/g;
    my $file_name = get_file_name($product, $lcd_name);

    if (($target_file_type eq "dtsi") || ($target_file_type eq "trebledto") || ($target_file_type eq "all"))
    {
        my $dtsi_file_path = get_out_dtsi_path($out_dtsi_file_path, $file_name);

        my $parser_para_string = $chip_plat . ' ' . $target_file_type . ' '
                            . $dtsi_file_path . ' ' . get_parse_xml_para(\@parse_xml_files);

        if (-e "$local_perl_path"){
            debug_print("changing path to local perl tool path: $local_perl_path\n");
            chdir $local_perl_path;
            system("./localperl/bin/perl $devkit_tool_path/lcd_kit_dtsi_parser.pl " . $parser_para_string);
            chdir $devkit_tool_path;
        }
        else
        {
            system("perl $devkit_tool_path/lcd_kit_dtsi_parser.pl " . $parser_para_string);
        }
    }

	$file_name =~ s/-/_/g;

    if (($target_file_type eq "head") || ($target_file_type eq "dto-head") || ($target_file_type eq "all"))
    {
        my $head_file_path = get_out_head_path($out_head_file_path, $file_name);

        my $parser_para_string = $chip_plat . ' ' . $target_file_type . ' '
                            . $head_file_path . ' ' . get_parse_xml_para(\@parse_xml_files);
        if (-e "$local_perl_path"){
            debug_print("changing path to local perl tool path: $local_perl_path\n");
            chdir $local_perl_path;
            #system("./localperl/bin/perl $devkit_tool_path/lcd_kit_head_parser.pl " . $parser_para_string);
            chdir $devkit_tool_path;
        }
        else
        {
            system("perl $devkit_tool_path/lcd_kit_head_parser.pl " . $parser_para_string);
        }
    }
    
    if ((($target_file_type eq "effect") || ($target_file_type eq "all")) && ($chip_plat eq "hisi"))
    {
        my $effect_file_path = get_out_effect_path($out_effect_file_path, $file_name);

        my $parser_para_string = $chip_plat . ' ' 
                            . $effect_file_path . ' ' . get_parse_xml_para(\@parse_xml_files);

        if (-e "$local_perl_path"){
            debug_print("changing path to local perl tool path: $local_perl_path\n");
            chdir $local_perl_path;
            system("./localperl/bin/perl $devkit_tool_path/lcd_kit_effect_parser.pl " . $parser_para_string);
            chdir $devkit_tool_path;
        }
        else
        {
            system("perl $devkit_tool_path/lcd_kit_effect_parser.pl " . $parser_para_string);
        }
    }
    #$file_name =~ s/-/_/g;
#    my @compatible_list = get_module_map_list("platform", \@plat_xmls, '/hwlcd/lcdlist/lcd');
    push(@lcd_file_names, $file_name);
    push(@lcd_map_group, $compatible.", "."\""."\/huawei,lcd_config/lcd_kit_".$file_name."\"".", ".$lcd_gpio.", ".$lcd_bdid);
    if ($lcd_gpio eq '0x0A')
    {
        push(@lcd_default_panel_map_group, $compatible.", "."\""."\/huawei,lcd_config/lcd_kit_".$file_name."\"".", ".$lcd_gpio.", \"".$product."\"");
    }
    if ($lcd_bdid ne '0')
    {
        push(@lcd_product_map_group, $lcd_bdid.", \"".$product."\"");
    }
    #hisi effect add
    my $lcd_xml_files = $lcd_xml_parser->parse_file($panel_xml);
    my $xml_node = '/hwlcd/PanelEntry/PanelCompatible';
    my $parse_str = parse_single_xml($lcd_xml_files, $xml_node);
    push(@lcd_effect_map_group, uc($file_name)."_PANEL, ".$parse_str.", ".$lcd_bdid);
}

if (($target_file_type eq "head") || ($target_file_type eq "dto-head") || ($target_file_type eq "all"))
{
    print "=====================parsing head file: lcd_kit_panels.h ======================\n";
	
    my $out_head_file = "$out_head_file_path/lcd_kit_panels.h";
    open (my $lcd_head_file, '>'.$out_head_file) or die "open $out_head_file fail!\n";
    
    print $lcd_head_file create_file_header();
    print $lcd_head_file "#ifndef _LCDKIT_PANELS__H_\n";
    print $lcd_head_file "#define _LCDKIT_PANELS__H_\n\n";
    
    if ($chip_plat eq 'hisi')
    {
       # print $lcd_head_file "#include \"lcdkit_disp.h\"";
    }

    foreach my $file_name (@lcd_file_names) {
       # print $lcd_head_file "\n#include \"$file_name.h\"";
    }
    
    #print $lcd_head_file create_lcd_enum(\@lcd_file_names);
    print $lcd_head_file create_lcd_map_struct(\@lcd_map_group);
    print $lcd_head_file create_default_panel_map_struct(\@lcd_default_panel_map_group);
    print $lcd_head_file create_product_map_struct(\@lcd_product_map_group);
    #print $lcd_head_file create_data_init_func(\@lcd_file_names, $chip_plat);
    if ($chip_plat eq 'qcom')
    {
        print $lcd_head_file create_qcom_panel_init();
        
        my $default_id0_gpio = get_platform_attr("lcd", \@plat_xmls, '/hwlcd/PanelEntry/GpioId0');
        my $default_id1_gpio = get_platform_attr("lcd", \@plat_xmls, '/hwlcd/PanelEntry/GpioId1');
        my $default_id_gpio = $default_id0_gpio . "," . $default_id1_gpio;
        my $specical_id_gpio = get_platform_attr("lcd", \@plat_xmls, '/hwlcd/PanelEntry/SpecIdGpio');
        $specical_id_gpio = $specical_id_gpio ne $parse_error_string ? $specical_id_gpio : "\"0,0,0\"";
        
        print $lcd_head_file create_qcom_gpio_init($default_id_gpio, $specical_id_gpio);
    }
    else
    {
        #print $lcd_head_file create_hisi_panel_init();
    }
    print $lcd_head_file "\n#endif /*_HW_LCD_PANELS__H_*/\n";
    
    close($lcd_head_file);
}

if ((($target_file_type eq "effect") || ($target_file_type eq "all")) && ($chip_plat eq 'hisi'))
{
    print "=====================parsing effect file: lcd_kit_effect.h ======================\n";
    
    my $out_effect_file = "$out_effect_file_path/lcd_kit_effect.h";
    open (my $lcd_effect_file, '>'.$out_effect_file) or die "open $out_effect_file fail!\n";
    
    print $lcd_effect_file create_file_header();
    
    print $lcd_effect_file "#ifndef _LCDKIT_EFFECT__H_\n";
    print $lcd_effect_file "#define _LCDKIT_EFFECT__H_\n\n";
    
    foreach my $file_name (@lcd_file_names) {
        print $lcd_effect_file "\n#include \"$file_name.h\"\n";
    }
    
    print $lcd_effect_file create_lcd_enum(\@lcd_file_names);
    print $lcd_effect_file create_lcd_effect_map_struct(\@lcd_effect_map_group);
    print $lcd_effect_file create_lcd_effect_data_func(\@lcd_file_names);
    print $lcd_effect_file create_hisi_panel_effect_init();    
    print $lcd_effect_file "\n#endif /*_LCDKIT_EFFECT__H_*/\n";
    
    close($lcd_effect_file);
}

if (($target_file_type eq "dtsi") || ($target_file_type eq "trebledto") || ($target_file_type eq "all"))
{
    foreach my $product_name (@product_names) {

		my $target_out_dtsi_folder = "$target_dtsi_file_path/$product_name";
        if (!-e "$target_out_dtsi_folder"){
            debug_print("target out dtsi file folder not exist, create it now!\n");
            system("mkdir -p " . $target_out_dtsi_folder);
        }
    
        my @file_temp = glob("$target_out_dtsi_folder/*.dtsi");
        for (@file_temp) {    unlink $_;    }
    
        print "=====================parsing dtsi file: $product_name devkit_lcd_kit.dtsi ======================\n";
        my $out_dtsi_file = "$target_out_dtsi_folder/devkit_lcd_kit.dtsi";
        open (my $lcd_dtsi_file, '>'.$out_dtsi_file) or die "open $out_dtsi_file fail!\n";
        print $lcd_dtsi_file create_file_header();
        my @dtsi_files = glob("$out_dtsi_file_path/*.dtsi");
        foreach my $dtsi_file (@dtsi_files) {
            my $dtsi_name = $dtsi_file;
            $dtsi_name =~ s/$out_dtsi_file_path//g;
            $dtsi_name =~ s/\///g;
            my @file_element = split /-/, $dtsi_name;
            my $match_patten = $file_element[0];
            my $new_file_name = $dtsi_name;
            $new_file_name =~ s/-/_/g;

            if ($product_name eq $match_patten) {
                if ($chip_plat eq 'hisi')
                {
                    print $lcd_dtsi_file "\n/include/ \"$new_file_name\"";
                }
                else
                {
                    print $lcd_dtsi_file "\n#include \"$new_file_name\"";
                }
                
                system("cp -f " . "$out_dtsi_file_path/$dtsi_name $target_out_dtsi_folder/$new_file_name");
            }
        }
        close($lcd_dtsi_file);

#	if ($chip_plat eq 'hisi')
#	{
#		print "=====================parsing dtsi file: lcd_kit_fastboot.dtsi ======================\n";  
#		my $fastboot_dtsi_file = "$target_dtsi_file_path/lcd_kit_fastboot.dtsi";
#		open (my $lcd_kit_fastboot_dtsi_file, '>'.$fastboot_dtsi_file) or die "open $fastboot_dtsi_file fail!\n";
#		print $lcd_kit_fastboot_dtsi_file create_file_header();
#		my @dtsi_files = glob("$out_dtsi_file_path/*.dtsi");
#
#		foreach my $lcd_names (@lcd_file_names) {
#			my $lcd_name = $lcd_names;
#			print $lcd_kit_fastboot_dtsi_file "\nhuawei,lcd_config/lcd_kit_" . $lcd_name . " {};";
#		}
#		close($lcd_kit_fastboot_dtsi_file);
#	}
    }
    
    system("rm -fr $out_dtsi_file_path");
}

exit 0;
