entity e is
end entity;

architecture a of e is
    function func1(x : integer) return integer;

    function func2(x : integer) return integer;
    function func2(x : bit) return integer;

    function func3(x : character) return integer;
    function func3(x : bit) return integer;

    function func4(x : integer) return integer;
    function func4(y : integer) return integer;  -- Error

    function func5(x : character) return integer;
    function func5(y : bit) return integer;

    type bool_vector is array (natural range <>) of boolean;

    function func6(x : bit_vector) return integer;
    function func6(x : bool_vector) return integer;

    type my_int is range 1 to 100;

    function func7(x : integer) return integer;
    function func7(x : my_int) return integer;

    function func8(x : bit_vector) return integer;
    function func8(x : string) return integer;
begin

    p1: process is
        variable x : integer;
    begin
        x := func1(1);                  -- OK

        x := func2(1);                  -- OK
        x := func2('1');                -- OK

        x := func3('a');                -- OK
        x := func3('1');                -- Error

        x := func5(x => '1');           -- OK
        x := func5(y => '1');           -- OK
        x := func5(z => '1');           -- Error
        x := func5("101");              -- Error

        x := func6("101");              -- OK
        x := func6(('1', '0'));         -- Error
        x := func6((true, false));      -- Error
        x := func6(bit_vector'(('1', '0')));  -- OK

        x := func7(5);                  -- Error

        x := func8("101");              -- Error
    end process;

    p2: process is
        procedure proc1(x : integer);

        procedure proc2(x : integer);
        procedure proc2(x : bit);

        procedure proc3(x : character);
        procedure proc3(x : bit);

        procedure proc4(x : integer);
        procedure proc4(y : integer);  -- Error

        procedure proc5(x : character);
        procedure proc5(y : bit);

        procedure proc6(x : bit_vector);
        procedure proc6(x : bool_vector);

        procedure proc7(x : integer);
        procedure proc7(x : my_int);

        procedure proc8(x : bit_vector);
        procedure proc8(x : string);

        variable v : bit_vector(1 to 3);
        variable proc8 : bit_vector(3 to 5);  -- Error
    begin
        proc1(1);                  -- OK

        proc2(1);                  -- OK
        proc2('1');                -- OK

        proc3('a');                -- OK
        proc3('1');                -- Error

        proc5(x => '1');           -- OK
        proc5(y => '1');           -- OK
        proc5(z => '1');           -- Error
        proc5("101");              -- Error

        proc6("101");              -- OK
        proc6(('1', '0'));         -- Error
        proc6((true, false));      -- Error
        proc6(bit_vector'(('1', '0')));  -- OK

        proc7(5);                  -- Error

        proc8("101");              -- Error
        v(1);                      -- Error
        foo(1, 2);                 -- Error
    end process;

    p3: process is
        type line is access string;
        variable l : line;
    begin
        assert now = 0 ns;              -- OK
        assert l = null;                -- OK
    end process;

    p4: process is
        type my_other_int is range 1 to 100;
        variable x : my_int;
    begin
        x := 5 * (2 + 4);               -- OK
    end process;

    p5: process is
        type table_t is array (bit) of character;
        constant table : table_t := ( '0' => '0',
                                      '1' => '1' );  -- OK
    begin
    end process;

    p6: process is
        constant FPO_LOG_MAX_ITERATIONS : integer := 9;
        type T_FPO_LOG_ALPHA is array (0 to FPO_LOG_MAX_ITERATIONS-1) of integer;
        variable alpha : T_FPO_LOG_ALPHA;
    begin
    end process;

    b7: block is
        constant WIDTH : integer := 5;
    begin
        gen: for i in 0 to WIDTH - 1 generate
        end generate;
    end block;

    b8: block is
        impure function get_foo return integer;
        attribute foreign of get_foo : function is "_get_foo";
    begin
    end block;

    p9: process is
        type unsigned is array (natural range <>) of bit;
        function "<"(x, y : unsigned) return boolean is  -- OK
        begin
            return x(0) = '0';
        end function;
        variable a, b : unsigned;
    begin
        assert a < b;                   -- OK (should be user-defined)
    end process;

    p10: process is
        procedure finish;

        procedure stop_impl(finish, have_status : boolean; status : integer) is
        begin
        end procedure;

        procedure stop(status : integer) is
        begin
            -- OK
            stop_impl(finish => false, have_status => true, status => status);
        end procedure;
    begin
    end process;

    p11: process is
        function foo return bit is
        begin
            return '1';
        end function;

        function foo return bit_vector is
        begin
            return "11";
        end function;

        variable x : bit_vector;
        variable y : bit;
    begin
        x := foo;                       -- OK
        y := foo;                       -- OK
    end process;

    b12: block is
        signal x : bit_vector(3 downto 0);
        signal y : integer;
    begin
        decode_y: with x select y <=    -- OK
            0 when X"0",
            1 when X"1",
            2 when X"2",
            3 when X"3";
    end block;

    p13: process is
    begin
        assert integer'value("5") = 5;  -- OK
    end process;

    b14: block is
        function foo return integer is
        begin
            return 1;
        end function;

        procedure check is
            type mytype is (foo);
        begin
            assert foo = foo;           -- Error
        end procedure;
    begin
    end block;

    b15: block is
        type mytype is (a, b);
        function "+"(x, y : mytype) return mytype is
            variable d : integer;
        begin
            d := "+".d;                 -- OK
            d := "+".x;                 -- Error
            return a;
        end function;
    begin
    end block;

    b16: block is
        -- From VESTS tc290.vhd
        type mytime is range 1 to 30
            units
                fs;
            end units;
    begin
        testing: process
            variable t,a   :mytime;
            variable b   :integer;
        begin
            a:=30 fs;
            b := 10;
            t:= a/b;
        end process;
    end block;

    p17: assert nothere(1) = 5;         -- Error

    p18: process is
        type myrec is record
            x, y : bit;
        end record;
        variable r : myrec;
    begin
        assert r.x = '1';               -- OK
    end process;

    p19: process is
        procedure p19_proc(x : in integer; y : out integer);
        function p19_func(x : integer) return integer;
    begin
        p19_proc(5, p19_func(y) => 4);  -- OK
    end process;

    p20: process is
        type bit_ptr is access bit;
        variable b : bit_ptr;
    begin
        assert b.all = '1';             -- OK
    end process;

    p21: process is
        subtype my_int is integer range 1 to 20;
    begin
        assert my_int'base'left = 1;    -- OK
    end process;

    p22: process is
        subtype my_int is integer range 1 to 5;
        constant c : bit_vector(1 to 10) := (
            my_int => '1', others => '0' );  -- OK
    begin
    end process;

end architecture;