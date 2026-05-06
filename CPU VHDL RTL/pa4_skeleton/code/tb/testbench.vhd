library ieee;
use ieee.std_logic_1164.all;
library std;
use std.env.all;

entity testbench is
    generic (
        MEM_INIT_FILE : string := "RType_ADD.out"
    );
end entity testbench;

architecture Behavioral of testbench is

    signal clk   : std_logic := '0';
    signal rst   : std_logic := '1';  --- active high Reset
    signal terminate : std_logic;  

    -- Instantiate DUT
    component PA4
        generic (
            MEM_INIT_FILE : string := "RType_ADD.out"
        );
        port (
            clk_in : in  std_logic;
            rst_in : in  std_logic;
            terminate  : out std_logic   
        );
    end component;

begin

    -- DUT mapping
    dut_inst : PA4
        generic map (
            MEM_INIT_FILE => MEM_INIT_FILE
        )
        port map (
            clk_in => clk,
            rst_in => rst,
            terminate  => terminate
        );

    -- Reset process
    reset_proc : process
    begin
        rst <= '1';        -- assert reset
        wait for 60 ps;
        rst <= '0';        -- release reset
        wait;
    end process;

    -- Clock process: 100 ps period
    clk_proc : process
    begin
        clk <= '0';
        wait for 50 ps;
        clk <= '1';
        wait for 50 ps;
    end process;

    -- terminate or timeout monitor: stop simulation when terminate=1 or time >= 500 ns
    stop_monitor : process
    begin
        wait for 0 ns; -- start immediately

        loop
            if terminate = '1' then
                -- Normal termination - terminate detected (test completed)
                wait for 50 ps;
                report "Simulation completed successfully (terminate detected)." severity note;
                stop;
            elsif now >= 500 ns then
                -- Timeout - test didn't complete in time
                report "ERROR: Timeout reached (500 ns). terminate not detected, stopping simulation!" severity failure;
                stop;
            else
                wait for 1 ps;  -- small polling interval
            end if;
        end loop;
    end process;


end architecture Behavioral;
