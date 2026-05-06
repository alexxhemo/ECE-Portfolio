library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library std;
use std.textio.all;
use ieee.std_logic_textio.all;

entity register_file is
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
        rst          : in  std_logic;   -- active-high asynchronous reset
        terminate    : in  std_logic;   -- ADDED for terminate-based dump
        read_data1   : out std_logic_vector(DATA_WIDTH-1 downto 0);
        read_data2   : out std_logic_vector(DATA_WIDTH-1 downto 0);
        read_data3   : out std_logic_vector(DATA_WIDTH-1 downto 0);
        read_data4   : out std_logic_vector(DATA_WIDTH-1 downto 0);
        read_data5   : out std_logic_vector(DATA_WIDTH-1 downto 0)
    );
end entity register_file;

architecture Behavioral of register_file is

    constant REG_COUNT : integer := 2 ** ADDR_WIDTH;

    --------------------------------------------------------------------
    -- RISC-V register ABI names
    --------------------------------------------------------------------
    subtype reg_name_t is string(1 to 4);
    type reg_name_array_t is array (0 to 31) of reg_name_t;

    constant REG_ABI_NAMES : reg_name_array_t := (
        0=>"zero",1=>"ra  ",2=>"sp  ",3=>"gp  ",4=>"tp  ",5=>"t0  ",6=>"t1  ",
        7=>"t2  ",8=>"s0  ",9=>"s1  ",10=>"a0  ",11=>"a1  ",12=>"a2  ",
        13=>"a3  ",14=>"a4  ",15=>"a5  ",16=>"a6  ",17=>"a7  ",
        18=>"s2  ",19=>"s3  ",20=>"s4  ",21=>"s5  ",22=>"s6  ",
        23=>"s7  ",24=>"s8  ",25=>"s9  ",26=>"s10 ",27=>"s11 ",
        28=>"t3  ",29=>"t4  ",30=>"t5  ",31=>"t6  "
    );

    type ram_type is array (0 to REG_COUNT-1) of std_logic_vector(DATA_WIDTH-1 downto 0);
    signal ram : ram_type := (others => (others => '0'));

    signal terminate_dump_done : std_logic := '0';

    --------------------------------------------------------------------
    -- Derive base name for REG_LOG_FILE
    --------------------------------------------------------------------
    function get_test_basename(filename : string) return string is
        variable last_sep : integer := filename'low - 1;
        variable last_dot : integer := filename'high + 1;
    begin
        for i in filename'range loop
            if (filename(i) = '/') or (filename(i) = '\') then
                last_sep := i;
            elsif filename(i) = '.' then
                last_dot := i;
            end if;
        end loop;

        if last_sep < filename'low then
            last_sep := filename'low - 1;
        end if;

        if (last_dot > filename'high) or (last_dot <= last_sep + 1) then
            last_dot := filename'high + 1;
        end if;

        return filename(last_sep + 1 to last_dot - 1);
    end function;

    constant REG_LOG_FILE : string :=
        "Logs/" & get_test_basename(MEM_INIT_FILE) & "_reg.log";

    --------------------------------------------------------------------
    -- PROCEDURE: dump registers to log
    --------------------------------------------------------------------
    procedure dump_register_log(
        constant logfilename : in string
    ) is
        file F : text;
        variable L : line;
    begin
        file_open(F, logfilename, write_mode);

        write(L, string'("----------------------------"));  writeline(F, L);
        write(L, string'("--- Register Dump ---"));       writeline(F, L);
        write(L, string'("----------------------------"));  writeline(F, L);

        write(L, string'("Index  Name  Value"));          writeline(F, L);
        write(L, string'("-----  ----  ----------------")); writeline(F, L);

        for j in 0 to 31 loop
            write(L, string'("x")); write(L, j);

            if j < 10 then write(L, string'("  "));
            else            write(L, string'(" "));
            end if;

            write(L, REG_ABI_NAMES(j));
            write(L, string'("  0x")); hwrite(L, ram(j));
            writeline(F, L);
        end loop;

        writeline(F, L);
        writeline(F, L);

        file_close(F);
    end procedure;

begin

    --------------------------------------------------------------------
    -- WRITE + RESET PROCESS
    --------------------------------------------------------------------
    process(clk, rst)
        variable idx : integer;
    begin
        if rst = '1' then
            for i in 0 to REG_COUNT-1 loop
                ram(i) <= (others => '0');
            end loop;

        elsif rising_edge(clk) then
            if we_1 = '1' and rd_1 /= "00000" then
                idx := to_integer(unsigned(rd_1));
                if idx < REG_COUNT then
                    ram(idx) <= write_data_1;
                end if;
            end if;

            if we_2 = '1' and rd_2 /= "00000" then
                idx := to_integer(unsigned(rd_2));
                if idx < REG_COUNT then
                    ram(idx) <= write_data_2;
                end if;
            end if;

        end if;
    end process;

    --------------------------------------------------------------------
    -- COMBINATIONAL READ PORTS
    --------------------------------------------------------------------
    process(rs1, ram)
        variable idx : integer;
    begin
        idx := to_integer(unsigned(rs1));
            if idx < REG_COUNT then
                read_data1 <= ram(idx);
            else
                read_data1 <= (others => '0');
            end if;
    end process;

    process(rs2, ram)   
        variable idx : integer;
    begin
        idx := to_integer(unsigned(rs2));
            if idx < REG_COUNT then
                read_data2 <= ram(idx);
            else
                read_data2 <= (others => '0');
            end if;
    end process;

    process(rs3, ram)   
        variable idx : integer;
    begin
        idx := to_integer(unsigned(rs3));
            if idx < REG_COUNT then
                read_data3 <= ram(idx);
            else
                read_data3 <= (others => '0');
            end if;
    end process;

    process(rs4, ram)   
        variable idx : integer;
    begin
        idx := to_integer(unsigned(rs4));
            if idx < REG_COUNT then
                read_data4 <= ram(idx);
            else
                read_data4 <= (others => '0');
            end if;
    end process;

    process(rs5, ram)   
        variable idx : integer;
    begin
        idx := to_integer(unsigned(rs5));
            if idx < REG_COUNT then
                read_data5 <= ram(idx);
            else
                read_data5 <= (others => '0');
            end if;
    end process;

    --------------------------------------------------------------------
    -- TERMINATE-TRIGGERED REGISTER DUMP
    --------------------------------------------------------------------
    terminate_trigger_dump : process
    begin
        wait until (terminate = '1');
        dump_register_log(REG_LOG_FILE);
        terminate_dump_done <= '1';
        wait;
    end process;

    --------------------------------------------------------------------
    -- FINAL DUMP AT 500 ns (only if NOT already dumped)
    --------------------------------------------------------------------
    final_dump : process
    begin
        wait for 500 ns;

        if terminate_dump_done /= '1' then
            dump_register_log(REG_LOG_FILE);
        end if;

        wait;
    end process;

end architecture Behavioral;
