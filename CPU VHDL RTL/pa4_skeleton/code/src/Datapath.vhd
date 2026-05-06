library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity Datapath is
    Port (
        clk_in          : in  std_logic;
        rst_in          : in  std_logic;
        pc_en           : in  std_logic;

        ALUSrcB         : in  std_logic;
        PCSrc           : in  std_logic;
        WBSrc           : in  std_logic;
        alu_contrl      : in  std_logic_vector(2 downto 0);

        data_mem_out    : in  std_logic_vector(31 downto 0);
        immediate       : in  std_logic_vector(31 downto 0);
        read_data1      : in  std_logic_vector(31 downto 0);
        read_data2      : in  std_logic_vector(31 downto 0);

        inst_addr       : out std_logic_vector(31 downto 0);
        dmem_addr       : out std_logic_vector(31 downto 0);
        regf_wd         : out std_logic_vector(31 downto 0);
        bne             : out std_logic
    );
end Datapath;

architecture Behavioral of Datapath is

    -- =============================
    -- Internal Signals
    -- =============================
    signal PC           : std_logic_vector(31 downto 0);
    signal PC_next      : std_logic_vector(31 downto 0);

    signal alu_input_B  : std_logic_vector(31 downto 0);
    signal alu_result   : std_logic_vector(31 downto 0);
    signal wb_data      : std_logic_vector(31 downto 0);
    
    signal shamt        : integer range 0 to 31;

begin

    -- =============================
    -- Instruction Address
    -- =============================
    inst_addr <= PC;

    -- =============================
    -- PC Source MUX (PCSrc)
    -- =============================
    process(PCSrc, immediate, PC)
    begin
        if PCSrc = '0' then
            PC_next <= std_logic_vector(unsigned(PC) + 4);
        else
            PC_next <= std_logic_vector(unsigned(PC) + unsigned(immediate));
        end if;
    end process;

    -- =============================
    -- PC Register with Enable
    -- =============================
    process(clk_in, rst_in)
    begin
        if rst_in = '1' then
            PC <= (others => '0');
        elsif rising_edge(clk_in) then
            if pc_en = '1' then
                PC <= PC_next(31 downto 0);
                -- PC <= PC_next(31 downto 1) & '0'; -- alignment
            end if;
        end if;
    end process;

    -- =============================
    -- ALU Input B MUX (ALUSrcB)
    -- =============================
    process(ALUSrcB, read_data2, immediate)
    begin
        if ALUSrcB = '0' then
            alu_input_B <= read_data2;
        else
            alu_input_B <= immediate;
        end if;
    end process;

    -- =============================
    -- ALU Logic
    -- =============================
    shamt <= to_integer(unsigned(alu_input_B(4 downto 0)));

    process(alu_contrl, read_data1, alu_input_B, shamt)
    begin
        case alu_contrl is
            when "000" =>  -- ADD
                alu_result <= std_logic_vector(signed(read_data1) + signed(alu_input_B));

            when "011" =>  -- SUB
                alu_result <= std_logic_vector(signed(read_data1) - signed(alu_input_B));

            when "001" =>  -- SLL
                alu_result <= std_logic_vector(shift_left(unsigned(read_data1), shamt));

            when "010" =>  -- SRL
                alu_result <= std_logic_vector(shift_right(unsigned(read_data1), shamt));
            
            when "100" =>  -- BNE
                if signed(read_data1) /= signed(alu_input_B) then
                    alu_result <= (31 downto 1 => '0') & '1';
                else
                    alu_result <= (others => '0');
                end if;

            when others =>
                alu_result <= (others => '0');

        end case;
    end process;

    -- Branch Flag (BNE)
    bne <= alu_result(0) when alu_contrl = "100" else '0';

    -- =============================
    -- Write Back MUX (WBSrc)
    -- =============================
    process(WBSrc, alu_result, data_mem_out)
    begin
        if WBSrc = '0' then
            wb_data <= alu_result;
        else
            wb_data <= data_mem_out;
        end if;
    end process;

    -- =============================
    -- Outputs
    -- =============================
    dmem_addr <= alu_result;
    regf_wd   <= wb_data;

end Behavioral;
