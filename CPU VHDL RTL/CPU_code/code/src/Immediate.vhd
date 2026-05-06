library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use work.itype_pkg.all;

entity Immediate is
    Port ( 
        Inst_In  : in  STD_LOGIC_VECTOR(31 downto 0);
        Imm_Out : out STD_LOGIC_VECTOR(31 downto 0)
    );
end Immediate;

architecture Behavioral of Immediate is
    signal opcode : STD_LOGIC_VECTOR(6 downto 0);
begin
    -- Extract opcode
    opcode <= Inst_In(6 downto 0);
    
    -- Combinational logic for immediate generation
    process(Inst_In, opcode)
    begin
        case opcode is
            -- Store instruction (S-type)
            when "0100011" =>
                Imm_Out <= (31 downto 12 => Inst_In(31)) & Inst_In(31 downto 25) & Inst_In(11 downto 7);
            
            -- Branch instruction (B-type)
            when "1100011" =>
                Imm_Out <= (31 downto 12 => Inst_In(31)) & Inst_In(7) & Inst_In(30 downto 25) & Inst_In(11 downto 8) & '0';
            
            -- JAL instruction (J-type)
            when "1101111" =>
                Imm_Out <= (31 downto 20 => Inst_In(31)) & Inst_In(19 downto 12) & Inst_In(20) & Inst_In(30 downto 21) & '0';
            
            -- LUI instruction (U-type)
            when "0110111" =>
                Imm_Out <= Inst_In(31 downto 12) & "000000000000";
            
            -- Default: I-type immediate
            when others =>
                Imm_Out <= (31 downto 12 => Inst_In(31)) & Inst_In(31 downto 20);
        end case;
    end process;

end architecture Behavioral;