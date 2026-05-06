library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity GEMM is
    port(
        clk, reset    : in std_logic;
        Gemm_start_1  : in std_logic;
        Gemm_start_2  : in std_logic;

          
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
end GEMM;

architecture Behavioral of GEMM is

    --enter your signals here!
    type gemm_state_type is (IDLE, MAC);
    signal state : gemm_state_type := IDLE;
    signal i : integer range 0 to 1 := 0;
    signal j : integer range 0 to 1 := 0;
    signal k : integer range 0 to 1 := 0;
    signal active_bank : std_logic := '0';
    signal mul_res : signed(63 downto 0) := (others => '0');
    signal acc_res : signed(31 downto 0) := (others => '0');
   
begin
    -----------------
    -- CONTROLLER FSM
    -----------------
    controller : process(clk, reset)
    begin
        --enter your VHDL code here
        if reset = '1' then
            state <= IDLE;
            i <= 0;
            j <= 0;
            k <= 0;
            active_bank <= '0';
            
        elsif rising_edge(clk) then
            case state is
                when IDLE =>
                    i <= 0;
                    j <= 0;
                    k <= 0;

                    if Gemm_start_1 = '1' then
                        state <= MAC;
                        active_bank <= '0';
                    elsif Gemm_start_2 = '1' then
                        state <= MAC;
                        active_bank <= '1';
                    end if;

                when MAC =>
                    if (i = 1 and j = 1 and k = 1) then
                        state <= IDLE;
                        i <= 0;
                        j <= 0;
                        k <= 0;

                    elsif k = 0 then
                        k <= 1;
                    
                    else
                        k <= 0;

                        if j = 0 then
                            j <= 1;
                        else
                            j <= 0;
                            
                            if i = 0 then
                                i <= 1;
                            else
                                i <= 0;
                            end if;
                        end if;
                    end if;
            end case;
        end if;
    end process;

    
    -----------------
    -- MAC Datapath
    -----------------
    datapath : process(state, active_bank, i, j, k, regf_A, regf_B, regf_C)
    begin
        --enter your VHDL code here
        A_addr <= (others => '0');
        B_addr <= (others => '0');
        C_addr <= (others => '0');
        gemm_output <= (others => '0');
        gemm_active_1 <= '0';
        gemm_active_2 <= '0';
        ready <= '0';
        mul_res <= (others => '0');
        acc_res <= (others => '0');

        case state is
            when IDLE =>
                ready <= '1';

            when MAC =>
                ready <= '0';
                mul_res <= signed(regf_A) * signed(regf_B);

                if active_bank = '0' then
                    gemm_active_1 <= '1';
                    A_addr <= std_logic_vector(to_unsigned(5 + i*2 + k, 5));
                    B_addr <= std_logic_vector(to_unsigned(9 + k*2 + j, 5));
                    C_addr <= std_logic_vector(to_unsigned(13 + i*2 + j, 5));
                else
                    gemm_active_2 <= '1';
                    A_addr <= std_logic_vector(to_unsigned(17 + i*2 + k, 5));
                    B_addr <= std_logic_vector(to_unsigned(21 + k*2 + j, 5));
                    C_addr <= std_logic_vector(to_unsigned(25 + i*2 + j, 5));
                end if;

                if k = 0 then
                    acc_res <= resize(signed(regf_A) * signed(regf_B), 32);
                    gemm_output <= std_logic_vector(resize(signed(regf_A) * signed(regf_B), 32));
                else
                    acc_res <= signed(regf_C) + resize(signed(regf_A) * signed(regf_B), 32);
                    gemm_output <= std_logic_vector(signed(regf_C) + resize(signed(regf_A) * signed(regf_B), 32));
                end if;
        end case;
    end process;

end Behavioral;