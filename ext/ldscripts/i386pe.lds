/* hl engine checks pe sections of the gamedll and faults on the default layout */
OUTPUT_FORMAT(pei-i386)
SEARCH_DIR("/mingw/mingw32/lib"); SEARCH_DIR("/mingw/lib"); SEARCH_DIR("/usr/local/lib"); SEARCH_DIR("/lib"); SEARCH_DIR("/usr/lib");
ENTRY(_mainCRTStartup)
SECTIONS
{
  .text  __image_base__ + __section_alignment__  :
  {
    *(.init)
    *(.text)
    *(.text.*)
    *(SORT(.text$*))
    *(.glue_7t)
    *(.glue_7)
   	___CTOR_LIST__ = .; __CTOR_LIST__ = . ;
			LONG (-1);
			*(EXCLUDE_FILE (*crtend.o) .ctors);
			*(.ctor);
			*(SORT(.ctors.*));
			*crtend.o (.ctors);
			LONG (0);
    	___DTOR_LIST__ = .; __DTOR_LIST__ = . ;
			LONG (-1);
			*(EXCLUDE_FILE (*crtend.o) .dtors);
			*(.dtor);
			*(SORT(.dtors.*));
			*crtend.o (.dtors);
			LONG (0);
     *(.fini)
     *(.gcc_exc)
    PROVIDE (etext = .);
    *(.gcc_except_table)
  /*}
  .rdata BLOCK(__section_alignment__) :
  {*/
    *(.rdata)
    *(SORT(.rdata$*))
  }
  .data BLOCK(__section_alignment__) :
  {
    __data_start__ = . ;
    *(.data)
    *(.data2)
    *(SORT(.data$*))
    __data_end__ = . ;
    *(.data_cygwin_nocopy)
    *(.eh_frame)
    ___RUNTIME_PSEUDO_RELOC_LIST__ = .;
    __RUNTIME_PSEUDO_RELOC_LIST__ = .;
    *(.rdata_runtime_pseudo_reloc)
    ___RUNTIME_PSEUDO_RELOC_LIST_END__ = .;
    __RUNTIME_PSEUDO_RELOC_LIST_END__ = .;
  /*}
  .bss BLOCK(__section_alignment__) :
  {*/
    __bss_start__ = . ;
    *(.bss)
    *(COMMON)
    __bss_end__ = . ;
  }
  .edata BLOCK(__section_alignment__) :
  {
    *(.edata)
  }
  .idata BLOCK(__section_alignment__) :
  {
    SORT(*)(.idata$2)
    SORT(*)(.idata$3)
    /* These zeroes mark the end of the import list.  */
    LONG (0); LONG (0); LONG (0); LONG (0); LONG (0);
    SORT(*)(.idata$4)
    SORT(*)(.idata$5)
    SORT(*)(.idata$6)
    SORT(*)(.idata$7)
  }
  /DISCARD/ :
  {
    *(.debug$S)
    *(.debug$T)
    *(.debug$F)
    *(.drectve)
  }
  .CRT BLOCK(__section_alignment__) :
  {
    ___crt_xc_start__ = . ;
    *(SORT(.CRT$XC*))  /* C initialization */
    ___crt_xc_end__ = . ;
    ___crt_xi_start__ = . ;
    *(SORT(.CRT$XI*))  /* C++ initialization */
    ___crt_xi_end__ = . ;
    ___crt_xl_start__ = . ;
    *(SORT(.CRT$XL*))  /* TLS callbacks */
    /* ___crt_xl_end__ is defined in the TLS Directory support code */
    ___crt_xp_start__ = . ;
    *(SORT(.CRT$XP*))  /* Pre-termination */
    ___crt_xp_end__ = . ;
    ___crt_xt_start__ = . ;
    *(SORT(.CRT$XT*))  /* Termination */
    ___crt_xt_end__ = . ;
  }
  .tls BLOCK(__section_alignment__) :
  {
    ___tls_start__ = . ;
    *(.tls)
    *(.tls$)
    *(SORT(.tls$*))
    ___tls_end__ = . ;
  }
  .endjunk BLOCK(__section_alignment__) :
  {
    /* end is deprecated, don't use it */
    PROVIDE (end = .);
    PROVIDE ( _end = .);
     __end__ = .;
  }
  .rsrc BLOCK(__section_alignment__) :
  {
    *(.rsrc)
    *(SORT(.rsrc$*))
  }
  .reloc BLOCK(__section_alignment__) :
  {
    *(.reloc)
  }
  .stab BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.stab)
  }
  .stabstr BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.stabstr)
  }
  /* DWARF 1.1 and DWARF 2.  */
  .debug_aranges BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_aranges)
  }
  .debug_pubnames BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_pubnames)
  }
  /* DWARF 2.  */
  .debug_info BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_info) *(.gnu.linkonce.wi.*)
  }
  .debug_abbrev BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_abbrev)
  }
  .debug_line BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_line)
  }
  .debug_frame BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_frame)
  }
  .debug_str BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_str)
  }
  .debug_loc BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_loc)
  }
  .debug_macinfo BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_macinfo)
  }
  /* SGI/MIPS DWARF 2 extensions.  */
  .debug_weaknames BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_weaknames)
  }
  .debug_funcnames BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_funcnames)
  }
  .debug_typenames BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_typenames)
  }
  .debug_varnames BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_varnames)
  }
  /* DWARF 3.  */
  .debug_ranges BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_ranges)
  }
  /* DWARF 4/5, keeps them after the loaded sections so .text stays first.  */
  .debug_addr BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_addr)
  }
  .debug_loclists BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_loclists)
  }
  .debug_rnglists BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_rnglists)
  }
  .debug_line_str BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_line_str)
  }
  .debug_str_offsets BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_str_offsets)
  }
  .debug_macro BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_macro)
  }
  .debug_types BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_types)
  }
  .debug_pubtypes BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_pubtypes)
  }
  .debug_names BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_names)
  }
  .debug_sup BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_sup)
  }
  .debug_cu_index BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_cu_index)
  }
  .debug_tu_index BLOCK(__section_alignment__) (NOLOAD) :
  {
    *(.debug_tu_index)
  }
}

