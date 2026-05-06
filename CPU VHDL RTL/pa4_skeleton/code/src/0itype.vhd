----------------------------------------------------------------------------------
-- ECE 3401 2026: Instruction type package
-- University of Connecticut
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

package itype_pkg is

  type instruction_type is (ADD_R, SUB_R, SRL_R, SLL_R, ADD_I, SRL_I, SLL_I, LW, SW, BNE_B, GEMM_1, GEMM_2, TERMIN, INVALID);
    
  constant R_type     : std_logic_vector(6 downto 0) := "0110011";
  constant I_type     : std_logic_vector(6 downto 0) := "0010011";
  constant Load_type  : std_logic_vector(6 downto 0) := "0000011";
  constant St_type    : std_logic_vector(6 downto 0) := "0100011";
  constant B_type     : std_logic_vector(6 downto 0) := "1100011";
  constant G_type_1   : std_logic_vector(6 downto 0) := "1111111";
  constant G_type_2   : std_logic_vector(6 downto 0) := "1111110";
  constant term_type  : std_logic_vector(6 downto 0) := "0000000";

end package itype_pkg;