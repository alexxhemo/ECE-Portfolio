library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;
use work.itype_pkg.all;

entity GEMM_RV5 is
    Port (
        -- GEMM-RiscV Interface related inputs and outputs
        -- from RiscV
        opcode          : in  std_logic_vector(6 downto 0);
        riscv_rs1       : in  std_logic_vector(4 downto 0);
        riscv_rs2       : in  std_logic_vector(4 downto 0);
        riscv_rd        : in  std_logic_vector(4 downto 0);
        MemWrite        : in  std_logic;
        RegWrite        : in  std_logic;
        riscv_regf_wd   : in std_logic_vector(31 downto 0);

        -- from GEMM
        gemm_active_1   : in  std_logic;
        gemm_active_2   : in  std_logic;
        gemm_output     : in  std_logic_vector(31 downto 0);
        A_addr_gemm     : in  std_logic_vector(4 downto 0);
        B_addr_gemm     : in  std_logic_vector(4 downto 0);
        C_addr_gemm     : in  std_logic_vector(4 downto 0);

        -- to PC logic
        pc_en           : out std_logic;

        -- to DMem 
        dmem_we         : out std_logic;

        -- to Register File
        regf_we_riscv   : out std_logic;
        regf_we_gemm    : out std_logic;
        regf_wd_riscv   : out std_logic_vector(31 downto 0);
        regf_wd_gemm    : out std_logic_vector(31 downto 0);
        rs1_addr_riscv  : out std_logic_vector(4 downto 0); 
        rs2_addr_riscv  : out std_logic_vector(4 downto 0);
        rs3_addr_gemm   : out std_logic_vector(4 downto 0);
        rs4_addr_gemm   : out std_logic_vector(4 downto 0);
        rs5_addr_gemm   : out std_logic_vector(4 downto 0);
        rd_addr_riscv   : out std_logic_vector(4 downto 0);
        rd_addr_gemm    : out std_logic_vector(4 downto 0)
    );
end GEMM_RV5;

architecture Behavioral of GEMM_RV5 is

    --enter your signals here!
    signal bank1_conflict : std_logic;
    signal bank2_conflict : std_logic;
    signal stall_riscv : std_logic;

begin


    --enter your VHDL codes here
    bank1_conflict <= '1' when
        ((opcode = Load_type) and 
            (((unsigned(riscv_rs1) >= to_unsigned(5, 5)) and (unsigned(riscv_rs1) <= to_unsigned(16, 5))) or
             ((unsigned(riscv_rd) >= to_unsigned(5, 5)) and (unsigned(riscv_rd) <= to_unsigned(16, 5))))) or
        ((opcode = St_type) and
            (((unsigned(riscv_rs1) >= to_unsigned(5, 5)) and (unsigned(riscv_rs1) <= to_unsigned(16, 5))) or
             ((unsigned(riscv_rs2) >= to_unsigned(5, 5)) and (unsigned(riscv_rs2) <= to_unsigned(16, 5)))))
        else '0';

    bank2_conflict <= '1' when
        ((opcode = Load_type) and 
            (((unsigned(riscv_rs1) >= to_unsigned(17, 5)) and (unsigned(riscv_rs1) <= to_unsigned(28, 5))) or
             ((unsigned(riscv_rd) >= to_unsigned(17, 5)) and (unsigned(riscv_rd) <= to_unsigned(28, 5))))) or
        ((opcode = St_type) and
            (((unsigned(riscv_rs1) >= to_unsigned(17, 5)) and (unsigned(riscv_rs1) <= to_unsigned(28, 5))) or
             ((unsigned(riscv_rs2) >= to_unsigned(17, 5)) and (unsigned(riscv_rs2) <= to_unsigned(28, 5)))))
        else '0';

    stall_riscv <= '1' when 
        (bank1_conflict = '1' and gemm_active_1 = '1') or
        (bank2_conflict = '1' and gemm_active_2 = '1')
        else '0';

    pc_en <= not stall_riscv;
    dmem_we <= MemWrite and not stall_riscv;
    regf_we_riscv <= RegWrite and not stall_riscv;
    regf_we_gemm <= gemm_active_1 or gemm_active_2;
    regf_wd_riscv <= riscv_regf_wd;
    regf_wd_gemm <= gemm_output;

    rs1_addr_riscv <= riscv_rs1;
    rs2_addr_riscv <= riscv_rs2;
    rd_addr_riscv <= riscv_rd;

    rs3_addr_gemm <= A_addr_gemm;
    rs4_addr_gemm <= B_addr_gemm;
    rs5_addr_gemm <= C_addr_gemm;
    rd_addr_gemm <= C_addr_gemm;
        


end Behavioral;
