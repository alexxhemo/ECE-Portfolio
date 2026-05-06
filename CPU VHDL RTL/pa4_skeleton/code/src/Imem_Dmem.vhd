library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_textio.all;
use std.textio.all;

entity Imem_Dmem is
    generic (
        MEM_INIT_FILE : string := "RType_ADD.out"
    );
    port (
        clk             : in  std_logic;
        inst_address    : in  std_logic_vector(31 downto 0);
        data_address    : in  std_logic_vector(31 downto 0);
        write_data      : in  std_logic_vector(31 downto 0);
        MemWrite        : in  std_logic;
        MemRead         : in  std_logic;
        terminate       : in  std_logic;
        inst_mem_out    : out std_logic_vector(31 downto 0);
        data_mem_out    : out std_logic_vector(31 downto 0)
    );
end entity Imem_Dmem;

architecture Behavioral of Imem_Dmem is
    type mem_array is array(0 to 17000) of std_logic_vector(31 downto 0);

    constant MEM_MAX_INDEX   : integer := 17000;
    constant DATA_START_ADDR : integer := 256;
    
    --------------------------------------------------------------------
    -- Helper: derive base test name from MEM_INIT_FILE
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

    constant TEST_BASENAME : string := get_test_basename(MEM_INIT_FILE);
    constant MEM_LOG_FILE  : string := "Logs/" & TEST_BASENAME & "_dmem.log";

    --------------------------------------------------------------------
    -- Memory initialization function
    --------------------------------------------------------------------
    impure function init_mem_from_file(filename : string) return mem_array is
        file mem_file      : text;
        variable file_line : line;
        variable temp_mem  : mem_array := (others => (others => '0'));
        variable temp_word : std_logic_vector(31 downto 0);
        variable inst_addr : integer := 0;
        variable data_addr : integer;
        variable status    : file_open_status;
        variable good_v    : boolean;
        variable found_sep : boolean := false;
        constant separator : std_logic_vector(31 downto 0) := (others => '1');
    begin
        file_open(status, mem_file, filename, read_mode);

        if status = open_ok then
            report "Loading unified memory from: " & filename;

            -- Load instructions until separator
            while not endfile(mem_file) and not found_sep and inst_addr <= MEM_MAX_INDEX loop
                readline(mem_file, file_line);

                if file_line'length > 0 then
                    read(file_line, temp_word, good_v);

                    if good_v then
                        if temp_word = separator then
                            found_sep := true;
                            report "Found separator, loaded " & integer'image(inst_addr) & " instructions";
                        else
                            temp_mem(inst_addr) := temp_word;
                            inst_addr := inst_addr + 1;
                        end if;
                    end if;
                end if;
            end loop;

            -- Load data after separator
            data_addr := DATA_START_ADDR;
            while not endfile(mem_file) and data_addr <= MEM_MAX_INDEX loop
                readline(mem_file, file_line);

                if file_line'length > 0 then
                    read(file_line, temp_word, good_v);

                    if good_v then
                        if temp_word = separator then
                            exit;
                        end if;
                        temp_mem(data_addr) := temp_word;
                        data_addr := data_addr + 1;
                    end if;
                end if;
            end loop;

            file_close(mem_file);
            report "Memory initialized: " & integer'image(inst_addr) & " instructions, " & 
                   integer'image(data_addr - DATA_START_ADDR) & " data words";
        else
            report "ERROR: Could not open file: " & filename severity error;
        end if;

        return temp_mem;
    end function;

    function find_last_initialized_index(mem_in : mem_array) return integer is
        variable last_idx : integer := DATA_START_ADDR - 1;
        constant ZERO_WORD : std_logic_vector(31 downto 0) := (others => '0');
    begin
        for idx in DATA_START_ADDR to MEM_MAX_INDEX loop
            if mem_in(idx) /= ZERO_WORD then
                last_idx := idx;
            end if;
        end loop;
        return last_idx;
    end function;

    constant INITIAL_MEM     : mem_array := init_mem_from_file(MEM_INIT_FILE);
    constant INITIAL_LOG_END : integer   := find_last_initialized_index(INITIAL_MEM);

    signal mem : mem_array := INITIAL_MEM;
    signal terminate_dump_done : std_logic := '0';

    --------------------------------------------------------------------
    -- Logging procedure (declared in architecture declarative region)
    --------------------------------------------------------------------
    procedure dump_memory_log(
        constant test_name        : string;
        constant start_idx        : integer;
        constant end_idx          : integer;
        constant initial_snapshot : mem_array;
        constant current_snapshot : mem_array
    ) is
        file mem_log : text;
        variable L : line;
        variable effective_start : integer := start_idx;
        variable effective_end   : integer := end_idx;
    begin
        if effective_start < 0 then
            effective_start := 0;
        end if;

        if effective_end < effective_start then
            effective_end := effective_start - 1;
        elsif effective_end > MEM_MAX_INDEX then
            effective_end := MEM_MAX_INDEX;
        end if;

        file_open(mem_log, MEM_LOG_FILE, write_mode);

        -- *** Header formatting ***
        write(L, string'("------------------------------------------------------------------------------------"));
        writeline(mem_log, L);

        write(L, string'("-------------------------------Test: "));
        write(L, test_name);
        writeline(mem_log, L);

        write(L, string'("------------------------------------------------------------------------------------"));
        writeline(mem_log, L);
        -- *** end header ***

        writeline(mem_log, L);  -- blank line

        -- Initial data section
        write(L, string'("----------------------------"));
        writeline(mem_log, L);
        write(L, string'("--- Initial Data in Memory"));
        writeline(mem_log, L);
        write(L, string'("----------------------------"));
        writeline(mem_log, L);
        writeline(mem_log, L);

        if effective_end >= effective_start then
            for addr in effective_start to effective_end loop
                write(L, string'("memory["));
                write(L, addr);
                write(L, string'("] = 0x"));
                hwrite(L, initial_snapshot(addr));
                writeline(mem_log, L);
            end loop;
        end if;

        writeline(mem_log, L);
        write(L, string'("----------------------------"));
        writeline(mem_log, L);
        write(L, string'("--- Updated Memory Dump ---"));
        writeline(mem_log, L);
        write(L, string'("----------------------------"));
        writeline(mem_log, L);
        writeline(mem_log, L);

        if effective_end >= effective_start then
            for addr in effective_start to effective_end loop
                write(L, string'("Memory["));
                write(L, addr);
                write(L, string'("] = 0x"));
                hwrite(L, current_snapshot(addr));
                writeline(mem_log, L);
            end loop;
        end if;
        
        writeline(mem_log, L);  -- blank line

        file_close(mem_log);
    end procedure;

begin

    --------------------------------------------------------------------
    -- MEMORY READ/WRITE LOGIC
    --------------------------------------------------------------------
    
    -- Synchronous write to data memory
    process(clk)
    begin
    if rising_edge(clk) then
        if MemWrite = '1' then
            if to_integer(unsigned(data_address)) <= MEM_MAX_INDEX then
                mem(to_integer(unsigned(data_address))) <= write_data;
            end if;
        end if;
    end if;
    end process;

    -- Combinational read for instruction memory
    process(inst_address, mem)
    begin
        if to_integer(unsigned(inst_address)) <= MEM_MAX_INDEX then
            inst_mem_out <= mem(to_integer(unsigned(inst_address)));
        else
            inst_mem_out <= (others => '0');
        end if;
    end process;

    -- Combinational read for data memory
    process(data_address, mem, MemRead)
    begin
        if MemRead = '1' and (to_integer(unsigned(data_address)) <= MEM_MAX_INDEX) then
            data_mem_out <= mem(to_integer(unsigned(data_address)));
        else
            -- Defaults to 0 if MemRead is '0' OR if address is out of bounds
            data_mem_out <= (others => '0');
        end if;
    end process;

    --------------------------------------------------------------------
    -- LOGGING LOGIC
    --------------------------------------------------------------------

    -- Initial log at time 0
    initial_log : process
    begin
        dump_memory_log(TEST_BASENAME, DATA_START_ADDR, INITIAL_LOG_END, INITIAL_MEM, INITIAL_MEM);
        wait;
    end process;


    terminate_trigger_dump : process
        variable log_end_idx_v : integer := INITIAL_LOG_END;
    begin
        wait until terminate = '1';

        if terminate_dump_done = '0' then
            -- compute last modified index
            for idx in DATA_START_ADDR to MEM_MAX_INDEX loop
                if mem(idx) /= INITIAL_MEM(idx) then
                    if idx > log_end_idx_v then
                        log_end_idx_v := idx;
                    end if;
                end if;
            end loop;

            dump_memory_log(TEST_BASENAME, DATA_START_ADDR, log_end_idx_v,
                            INITIAL_MEM, mem);

            terminate_dump_done <= '1';   -- <── mark as dumped
        end if;

        wait;
    end process;


    -- Final dump at 500ns
    final_dump_process : process
        variable log_end_idx_v : integer := INITIAL_LOG_END;
    begin
        wait for 500 ns;

        --  Only if terminate never dumped
        if terminate_dump_done /= '1' then 
            -- Find last modified address
            for idx in DATA_START_ADDR to MEM_MAX_INDEX loop
                if mem(idx) /= INITIAL_MEM(idx) then
                    if idx > log_end_idx_v then
                        log_end_idx_v := idx;
                    end if;
                end if;
            end loop;

            dump_memory_log(TEST_BASENAME, DATA_START_ADDR, log_end_idx_v,
                            INITIAL_MEM, mem);
        end if;

        wait;  -- Process ends after dump
    end process;

end architecture Behavioral;