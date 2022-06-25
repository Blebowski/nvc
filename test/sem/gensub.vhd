entity gensub is
end entity;

architecture test of gensub is
begin

    b1: block is
        function adder generic (n : integer) (x : integer) return integer is
        begin
            return x + n;               -- OK
        end function;

        function add1 is new adder generic map (1);  -- OK

        constant c1 : integer := add1(2);  -- OK
    begin
    end block;

    b2: block is
        function not_generic return integer;  -- OK
        function error1 is new not_generic generic map (1);  -- Error

        function no_body generic (n : integer) return integer;
        function error2 is new no_body generic map (1);  -- Error
    begin
    end block;

    b3: block is
        function adder generic (type t;
                                function "+"(l, r : t) return t is <>;
                                n : t) (x : t) return t is  -- OK
        begin
            return x + n;
        end function;

        function add1_int is new adder generic map (t => integer, n => 1);
        function add1_real is new adder generic map (t => real, n => 1.0);

        constant c1 : integer := add1_int(integer'(1));  -- OK
        constant c2 : real := add1_real(real'(1.0));  -- OK

        function add_error1 is new adder generic map (t => string, n => 1);  -- Error

        constant c3 : integer := adder(4); -- Error

        procedure do_stuff generic (x : integer) is
        begin
        end procedure;
    begin
        do_stuff;  -- Error
    end block;

    b4: block is
        function outer generic (type t) (x : t) return t is
            function inner generic (type q; y : q) return q is
            begin
                return y + t;  -- Error
            end function;

            function inner_inst is new inner generic map (q => t, y => x); -- Error
        begin
            return inner_inst;
        end function;
     begin
     end block;

end architecture;