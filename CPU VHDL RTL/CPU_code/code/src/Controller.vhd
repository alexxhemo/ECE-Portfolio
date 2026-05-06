library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;
use work.itype_pkg.all;

entity Controller is
    port (
        opcode       : in  std_logic_vector(6 downto 0);
        funct3       : in  std_logic_vector(2 downto 0);
        funct7       : in  std_logic_vector(6 downto 0);
        BNE          : in  std_logic;

        Gemm_start_1 : out std_logic;
        Gemm_start_2 : out std_logic;
        PCSrc        : out std_logic;
        ALUSrcB      : out std_logic;
        RegWrite     : out std_logic;
        MemWrite     : out std_logic;
        MemRead      : out std_logic;
        WBSrc        : out std_logic;
        ALUCtl       : out std_logic_vector(2 downto 0)   
    );
end entity Controller;

architecture Behavioral of Controller is
    signal Inst_Type : instruction_type;
begin

-- Decode instruction into types
process(opcode, funct3, funct7)
begin
    case opcode is
        when R_type =>
            case funct7 is
                when "0000000" =>
                    case funct3 is
                        when "000" => Inst_Type <= ADD_R;
                        when "001" => Inst_Type <= SLL_R;
                        when "101" => Inst_Type <= SRL_R;
                        when others => Inst_Type <= INVALID;
                    end case;
                when "0100000" =>
                    case funct3 is
                        when "000" => Inst_Type <= SUB_R;
                        when others => Inst_Type <= INVALID;
                    end case;
                when others => Inst_Type <= INVALID;
            end case;
        when I_type =>
            case funct3 is
                when "000" => Inst_Type <= ADD_I;
                when "001" => Inst_Type <= SLL_I;
                when "101" => Inst_Type <= SRL_I;
                when others => Inst_Type <= INVALID;
            end case;
        when Load_type =>
            Inst_Type <= LW;
        when St_type =>
            Inst_Type <= SW;
        when B_type =>
            Inst_Type <= BNE_B;
        when term_type =>
            Inst_Type <= TERMIN;
        --enter your VHDL code here
        when G_type_1 =>
            Inst_Type <= GEMM_1;
        when G_type_2 =>
            Inst_Type <= GEMM_2;

        when others =>
            null;
    end case;
end process;

-- generate control signals based on instruction type and BNE outcome
process(Inst_Type, BNE)
begin
    -- DEFAULT VALUES FOR ALL CONTROL SIGNALS
    Gemm_start_1 <= '0';
    Gemm_start_2 <= '0';
    PCSrc        <= '0';
    ALUSrcB      <= '0';
    RegWrite     <= '0';
    MemWrite     <= '0';
    MemRead      <= '0';
    WBSrc        <= '0';
    ALUCtl       <= "000";
    case Inst_Type is
        when ADD_R => ALUCtl <= "000"; ALUSrcB <= '0'; RegWrite <= '1'; WBSrc <= '0'; -- add
        when SLL_R => ALUCtl <= "001"; ALUSrcB <= '0'; RegWrite <= '1'; WBSrc <= '0'; -- sll
        when SRL_R => ALUCtl <= "010"; ALUSrcB <= '0'; RegWrite <= '1'; WBSrc <= '0'; -- srl
        when SUB_R => ALUCtl <= "011"; ALUSrcB <= '0'; RegWrite <= '1'; WBSrc <= '0'; -- sub

        when ADD_I => ALUCtl <= "000"; ALUSrcB <= '1'; RegWrite <= '1'; WBSrc <= '0'; -- addi
        when SLL_I => ALUCtl <= "001"; ALUSrcB <= '1'; RegWrite <= '1'; WBSrc <= '0'; -- slli
        when SRL_I => ALUCtl <= "010"; ALUSrcB <= '1'; RegWrite <= '1'; WBSrc <= '0'; -- srli

        when LW    => ALUCtl <= "000"; ALUSrcB <= '1'; RegWrite <= '1'; WBSrc <= '1'; MemRead  <= '1'; -- lw
        when SW    => ALUCtl <= "000"; ALUSrcB <= '1'; MemWrite  <= '1'; -- sw

        when BNE_B => ALUCtl <= "100"; -- bne
            if BNE = '1' then
                PCSrc <= '1'; -- taken branch
            end if;
        
        --enter your VHDL code here
        when GEMM_1 => Gemm_start_1 <= '1';
        when GEMM_2 => Gemm_start_2 <= '1';
        
        when others => null;
    end case;
end process;

end architecture Behavioral;
