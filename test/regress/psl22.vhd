entity psl22 is
end entity;

architecture tb of psl22 is

    signal clk : natural;

    signal a : bit;
    signal b : bit;
    signal c : bit;
    signal d : bit;

    signal glob : natural := 5;

begin

    clkgen: clk <= clk + 1 after 1 ns when clk < 10;

    -- psl default clock is clk'delayed(0 ns)'event;

    -- psl sequence my_seq(numeric x) is {x = 5;c};

    -- psl cover {a;my_seq(5);d};

    -- p s l cover {a;b;glob = 5;c;d};

end architecture;
