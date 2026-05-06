library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.itype_pkg.all;

entity PA4 is
    generic (
        MEM_INIT_FILE : string := "RType_ADD.out"
    );
    port (
        clk_in : in  std_logic;
        rst_in : in  std_logic;
        terminate : out std_logic
    );
end entity PA4;

architecture Behavioral of PA4 is

    -- =====================================================
    -- Internal Signals
    -- =====================================================
    signal instruction     : std_logic_vector(31 downto 0);
    signal regf_rd1        : std_logic_vector(31 downto 0);
    signal regf_rd2        : std_logic_vector(31 downto 0);
    signal immediate_out   : std_logic_vector(31 downto 0);
    signal regf_wd         : std_logic_vector(31 downto 0);
    signal rv5_regf_wd     : std_logic_vector(31 downto 0);
    signal gemm_regf_wd    : std_logic_vector(31 downto 0);


    signal Gemm_start_1    : std_logic;
    signal Gemm_start_2    : std_logic;
    signal ALUSrcB         : std_logic;
    signal PCSrc           : std_logic;
    signal RegWrite        : std_logic;
    signal MemWrite        : std_logic;
    signal MemRead         : std_logic;
    signal WBSrc           : std_logic;
    signal alu_contrl      : std_logic_vector(2 downto 0);

    signal dmem_addr       : std_logic_vector(31 downto 0);
    signal data_mem_out    : std_logic_vector(31 downto 0);

    signal bne             : std_logic;
    signal pc_en           : std_logic;
    signal inst_addr       : std_logic_vector(31 downto 0);
    signal inst_addr_shifted    : std_logic_vector(31 downto 0);

    -- GEMM signals
    signal gemm_ready    : std_logic;
    
    signal A_addr_gemm   : std_logic_vector(4 downto 0);
    signal B_addr_gemm   : std_logic_vector(4 downto 0);
    signal C_addr_gemm   : std_logic_vector(4 downto 0);

    signal regf_rd3      : std_logic_vector(31 downto 0);
    signal regf_rd4      : std_logic_vector(31 downto 0);
    signal regf_rd5      : std_logic_vector(31 downto 0);
    signal gemm_output   : std_logic_vector(31 downto 0);
    signal gemm_active_1 : std_logic;
    signal gemm_active_2 : std_logic;
    signal rs1_addr      : std_logic_vector(4 downto 0); 
    signal rs2_addr      : std_logic_vector(4 downto 0); 
    signal rs3_addr      : std_logic_vector(4 downto 0); 
    signal rs4_addr      : std_logic_vector(4 downto 0);
    signal rs5_addr      : std_logic_vector(4 downto 0);  
    signal rd1_addr      : std_logic_vector(4 downto 0);
    signal rd2_addr      : std_logic_vector(4 downto 0);  

    signal regf_we_riscv : std_logic;
    signal regf_we_gemm  : std_logic; 
    signal dmem_we       : std_logic;

    -- =====================================================
    -- Component Declarations
    -- =====================================================

    component Imem_Dmem
        generic (
            MEM_INIT_FILE : string := "RType_ADD.out"
        );
        port (
            clk          : in  std_logic;
            inst_address : in  std_logic_vector(31 downto 0);
            data_address : in  std_logic_vector(31 downto 0);
            write_data   : in  std_logic_vector(31 downto 0);
            MemWrite     : in  std_logic;
            MemRead      : in  std_logic;
            terminate    : in  std_logic;
            inst_mem_out : out std_logic_vector(31 downto 0);
            data_mem_out : out std_logic_vector(31 downto 0)
        );
    end component;

    component register_file
        generic (
            DATA_WIDTH    : integer := 32;
            ADDR_WIDTH    : integer := 5;
            MEM_INIT_FILE : string  := "RType_ADD.out"
        );
        port (
            write_data_1 : in  std_logic_vector(DATA_WIDTH-1 downto 0);
            write_data_2 : in  std_logic_vector(DATA_WIDTH-1 downto 0);
            rs1          : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            rs2          : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            rs3          : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            rs4          : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            rs5          : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            rd_1         : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            rd_2         : in  std_logic_vector(ADDR_WIDTH-1 downto 0);
            we_1         : in  std_logic;
            we_2         : in  std_logic;
            clk          : in  std_logic;
            rst          : in  std_logic;
            terminate    : in  std_logic;
            read_data1   : out std_logic_vector(DATA_WIDTH-1 downto 0);
            read_data2   : out std_logic_vector(DATA_WIDTH-1 downto 0);
            read_data3   : out std_logic_vector(DATA_WIDTH-1 downto 0);
            read_data4   : out std_logic_vector(DATA_WIDTH-1 downto 0);
            read_data5   : out std_logic_vector(DATA_WIDTH-1 downto 0)

        );
    end component;

    component Immediate
        port (
            Inst_In : in  std_logic_vector(31 downto 0);
            Imm_Out : out std_logic_vector(31 downto 0)
        );
    end component;

    component Controller
        port (
            opcode      : in  std_logic_vector(6 downto 0);
            funct3      : in  std_logic_vector(2 downto 0);
            funct7      : in  std_logic_vector(6 downto 0);
            BNE         : in  std_logic;
            Gemm_start_1: out std_logic;
            Gemm_start_2: out std_logic;
            PCSrc       : out std_logic;
            ALUSrcB     : out std_logic;
            RegWrite    : out std_logic;
            MemWrite    : out std_logic;
            MemRead     : out std_logic;
            WBSrc       : out std_logic;
            ALUCtl      : out std_logic_vector(2 downto 0)
        );
    end component;

    component Datapath
        port (
            clk_in       : in  std_logic;
            rst_in       : in  std_logic;
            pc_en        : in  std_logic;
            ALUSrcB      : in  std_logic;
            PCSrc        : in  std_logic;
            WBSrc        : in  std_logic;
            alu_contrl   : in  std_logic_vector(2 downto 0);
            data_mem_out : in  std_logic_vector(31 downto 0);
            immediate    : in  std_logic_vector(31 downto 0);
            read_data1   : in  std_logic_vector(31 downto 0);
            read_data2   : in  std_logic_vector(31 downto 0);
            inst_addr    : out std_logic_vector(31 downto 0);
            dmem_addr    : out std_logic_vector(31 downto 0);
            regf_wd      : out std_logic_vector(31 downto 0);
            bne          : out std_logic
        );
    end component;

    component GEMM_RV5
        port (
            opcode          : in  std_logic_vector(6 downto 0);
            riscv_rs1       : in  std_logic_vector(4 downto 0);
            riscv_rs2       : in  std_logic_vector(4 downto 0);
            riscv_rd        : in  std_logic_vector(4 downto 0);
            MemWrite        : in  std_logic;
            RegWrite        : in  std_logic;
            riscv_regf_wd   : in  std_logic_vector(31 downto 0);
            gemm_active_1   : in  std_logic;
            gemm_active_2   : in  std_logic;
            gemm_output     : in  std_logic_vector(31 downto 0);
            A_addr_gemm     : in  std_logic_vector(4 downto 0);
            B_addr_gemm     : in  std_logic_vector(4 downto 0);
            C_addr_gemm     : in  std_logic_vector(4 downto 0);
            pc_en           : out std_logic;
            dmem_we         : out std_logic;
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
    end component;

    component GEMM
        port (
            clk           : in  std_logic;                  
            reset         : in  std_logic;
            Gemm_start_1  : in  std_logic;  
            Gemm_start_2  : in  std_logic;  
            regf_A        : in std_logic_vector(31 downto 0);
            regf_B        : in std_logic_vector(31 downto 0);
            regf_C        : in std_logic_vector(31 downto 0);
  
            A_addr        : out std_logic_vector(4 downto 0);
            B_addr        : out std_logic_vector(4 downto 0);
            C_addr        : out std_logic_vector(4 downto 0);
            gemm_output   : out std_logic_vector(31 downto 0);
            gemm_active_1 : out std_logic;
            gemm_active_2 : out std_logic;
            ready         : out std_logic
        );
    end component;

begin

    -- =====================================================
    -- Address shifting (equivalent to >>2) and Halt Logic
    -- =====================================================
    inst_addr_shifted  <= std_logic_vector(shift_right(unsigned(inst_addr), 2));

    terminate <= '1' when instruction = x"00100013" else '0';
    -- =====================================================
    -- Unified Instruction + Data Memory
    -- =====================================================
    inst_data_mem : Imem_Dmem
        generic map (
            MEM_INIT_FILE => MEM_INIT_FILE
        )
        port map (
            clk          => clk_in,
            inst_address => inst_addr_shifted,
            data_address => dmem_addr,
            write_data   => regf_rd2,
            MemWrite     => dmem_we,
            MemRead      => MemRead,
            terminate    => terminate,
            inst_mem_out => instruction,
            data_mem_out => data_mem_out
        );

    -- =====================================================
    -- Register File
    -- =====================================================
    reg_file_inst : register_file
        generic map (
            DATA_WIDTH    => 32,
            ADDR_WIDTH    => 5,
            MEM_INIT_FILE => MEM_INIT_FILE
        )
        port map (
            write_data_1 => rv5_regf_wd,
            write_data_2 => gemm_regf_wd,
            rs1          => rs1_addr,
            rs2          => rs2_addr,
            rs3          => rs3_addr,
            rs4          => rs4_addr,
            rs5          => rs5_addr,
            rd_1         => rd1_addr,
            rd_2         => rd2_addr,
            we_1         => regf_we_riscv,
            we_2         => regf_we_gemm,
            clk          => clk_in,
            rst          => rst_in,
            terminate    => terminate,
            read_data1   => regf_rd1,
            read_data2   => regf_rd2,
            read_data3   => regf_rd3,
            read_data4   => regf_rd4,
            read_data5   => regf_rd5
        );


    -- =====================================================
    -- IMM GEN
    -- =====================================================
    immediate_unit : Immediate
        port map (
            Inst_In => instruction,
            Imm_Out => immediate_out
        );

    -- =====================================================
    -- Control Unit
    -- =====================================================
    control_unit : Controller
        port map (
            opcode       => instruction(6 downto 0),
            funct3       => instruction(14 downto 12),
            funct7       => instruction(31 downto 25),
            BNE          => bne,
            Gemm_start_1 => Gemm_start_1,
            Gemm_start_2 => Gemm_start_2,
            PCSrc        => PCSrc,
            ALUSrcB      => ALUSrcB,
            RegWrite     => RegWrite,
            MemWrite     => MemWrite,
            MemRead      => MemRead,
            WBSrc        => WBSrc,
            ALUCtl       => alu_contrl
        );

    -- =====================================================
    -- DATAPATH
    -- =====================================================
    datapath_inst : Datapath
        port map (
            clk_in       => clk_in,
            rst_in       => rst_in,
            pc_en        => pc_en,
            ALUSrcB      => ALUSrcB,
            PCSrc        => PCSrc,
            WBSrc        => WBSrc,
            alu_contrl   => alu_contrl,
            data_mem_out => data_mem_out,
            immediate    => immediate_out,
            read_data1   => regf_rd1,
            read_data2   => regf_rd2,
            inst_addr    => inst_addr,
            dmem_addr    => dmem_addr,
            regf_wd      => regf_wd,
            bne          => bne
        );    

    -- =====================================================
    -- GEMM-RiscV DATAPATH
    -- =====================================================

    gemm_rv5_datapath_inst : GEMM_RV5
        port map (
            opcode          => instruction(6 downto 0),
            riscv_rs1       => instruction(19 downto 15),
            riscv_rs2       => instruction(24 downto 20),
            riscv_rd        => instruction(11 downto 7),
            MemWrite        => MemWrite,
            RegWrite        => RegWrite,
            riscv_regf_wd   => regf_wd,
            gemm_active_1   => gemm_active_1,
            gemm_active_2   => gemm_active_2,
            gemm_output     => gemm_output,
            A_addr_gemm     => A_addr_gemm,
            B_addr_gemm     => B_addr_gemm,
            C_addr_gemm     => C_addr_gemm,
            pc_en           => pc_en,
            dmem_we         => dmem_we,
            regf_we_riscv   => regf_we_riscv,
            regf_we_gemm    => regf_we_gemm,
            regf_wd_riscv   => rv5_regf_wd,
            regf_wd_gemm    => gemm_regf_wd,
            rs1_addr_riscv  => rs1_addr,
            rs2_addr_riscv  => rs2_addr,
            rs3_addr_gemm   => rs3_addr,
            rs4_addr_gemm   => rs4_addr,
            rs5_addr_gemm   => rs5_addr,
            rd_addr_riscv   => rd1_addr,
            rd_addr_gemm    => rd2_addr
        );    


    -- =====================================================
    -- GEMM
    -- =====================================================
    gemm_inst : GEMM
        port map (
            clk           => clk_in,
            reset         => rst_in,
            Gemm_start_1  => Gemm_start_1,
            Gemm_start_2  => Gemm_start_2,
            regf_A        => regf_rd3,
            regf_B        => regf_rd4,
            regf_C        => regf_rd5,
            A_addr        => A_addr_gemm,
            B_addr        => B_addr_gemm,
            C_addr        => C_addr_gemm,
            gemm_output   => gemm_output,
            gemm_active_1 => gemm_active_1,
            gemm_active_2 => gemm_active_2,
            ready         => gemm_ready
        );


end architecture Behavioral;
