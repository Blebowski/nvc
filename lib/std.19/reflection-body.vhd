-------------------------------------------------------------------------------
--  Copyright (C) 2023  Nick Gasson
--
--  Licensed under the Apache License, Version 2.0 (the "License");
--  you may not use this file except in compliance with the License.
--  You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
--  Unless required by applicable law or agreed to in writing, software
--  distributed under the License is distributed on an "AS IS" BASIS,
--  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--  See the License for the specific language governing permissions and
--  limitations under the License.
-------------------------------------------------------------------------------

package body reflection is

    type string_ptr is access string;

    type internal_type;
    type type_ptr is access internal_type;

    type cache_elem_t is record
        f_type    : type_ptr;
        f_subtype : subtype_mirror;
    end record;

    type cache_ptr is access cache_elem_t;

    type internal_cache_pt is protected
        -- No methods, used internally
    end protected;

    type internal_cache_pt is protected body
        variable f_canary1       : integer := 16#deadbeef#;
        variable f_subtype_cache : cache_ptr;
        variable f_num_subtypes  : natural;
        variable f_max_subtypes  : natural;
        variable f_canary2       : integer := 16#cafebabe#;
    end protected body;

    shared variable cache : internal_cache_pt;

    ---------------------------------------------------------------------------

    type enumeration_value_mirror_pt is protected body
        variable f_owner   : value_mirror;
        variable f_subtype : enumeration_subtype_mirror;
        variable f_pos     : natural;
        variable f_image   : string_ptr;

        impure function get_subtype_mirror return enumeration_subtype_mirror is
        begin
            return f_subtype;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            return f_owner;
        end function;

        impure function pos return integer is
        begin
            return f_pos;
        end function;

        impure function image return string is
        begin
            return f_image.all;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type enumeration_subtype_mirror_pt is protected body
        variable f_owner : subtype_mirror;

        type evm_array is array (natural_index range <>) of enumeration_value_mirror;
        type evm_array_ptr is access evm_array;

        variable f_literals : evm_array_ptr;

        impure function to_subtype_mirror return subtype_mirror is
        begin
            return f_owner;
        end function;

        impure function enumeration_literal (literal_idx : natural_index)
            return enumeration_value_mirror is
        begin
            return f_literals.all(literal_idx);
        end function;

        impure function enumeration_literal (literal_name : string)
            return enumeration_value_mirror
        is
            variable tmp : enumeration_value_mirror;
        begin
            for i in f_literals.all'range loop
                tmp := f_literals.all(i);
                return tmp when tmp.image = literal_name;
            end loop;

            report literal_name
                & " does not denote an enumeration literal of type "
                & simple_name
                severity error;
            return null;
        end function;

        impure function simple_name return string is
        begin
            return f_owner.simple_name;
        end function;

        impure function left return enumeration_value_mirror is
        begin
            return f_literals.all(f_literals.all'left);
        end function;

        impure function right return enumeration_value_mirror is
        begin
            return f_literals.all(f_literals.all'right);
        end function;

        impure function low return enumeration_value_mirror is
        begin
            return left;
        end function;

        impure function high return enumeration_value_mirror is
        begin
            return right;
        end function;

        impure function length return positive_index is
        begin
            return positive_index(f_literals.all'length);
        end function;

        impure function ascending return boolean is
        begin
            return true;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type integer_value_mirror_pt is protected body
        variable f_owner   : value_mirror;
        variable f_subtype : integer_subtype_mirror;
        variable f_value   : integer;

        impure function get_subtype_mirror return integer_subtype_mirror is
        begin
            return f_subtype;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            return f_owner;
        end function;

        impure function value return integer is
        begin
            return f_value;
        end function;

        impure function image return string is
        begin
            return integer'image(f_value);
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type integer_subtype_mirror_pt is protected body
        variable f_owner     : subtype_mirror;
        variable f_left      : integer_value_mirror;
        variable f_right     : integer_value_mirror;
        variable f_low       : integer_value_mirror;
        variable f_high      : integer_value_mirror;
        variable f_ascending : boolean;

        impure function to_subtype_mirror return subtype_mirror is
        begin
            return f_owner;
        end function;

        impure function simple_name return string is
        begin
            return f_owner.simple_name;
        end function;

        impure function left return integer_value_mirror is
        begin
            return f_left;
        end function;

        impure function right return integer_value_mirror is
        begin
            return f_right;
        end function;

        impure function low return integer_value_mirror is
        begin
            return f_low;
        end function;

        impure function high return integer_value_mirror is
        begin
            return f_high;
        end function;

        impure function length return index is
        begin
            return maximum(index(f_high.value - f_low.value + 1), 0);
        end function;

        impure function ascending return boolean is
        begin
            return f_ascending;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type floating_value_mirror_pt is protected body
        variable f_owner   : value_mirror;
        variable f_subtype : floating_subtype_mirror;
        variable f_value   : real;

        impure function get_subtype_mirror return floating_subtype_mirror is
        begin
            return f_subtype;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            return f_owner;
        end function;

        impure function value return real is
        begin
            return f_value;
        end function;

        impure function image return string is
        begin
            return real'image(f_value);
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type floating_subtype_mirror_pt is protected body
        variable f_owner     : subtype_mirror;
        variable f_left      : floating_value_mirror;
        variable f_right     : floating_value_mirror;
        variable f_low       : floating_value_mirror;
        variable f_high      : floating_value_mirror;
        variable f_ascending : boolean;

        impure function to_subtype_mirror return subtype_mirror is
        begin
            return f_owner;
        end function;

        impure function simple_name return string is
        begin
            return f_owner.simple_name;
        end function;

        impure function left return floating_value_mirror is
        begin
            return f_left;
        end function;

        impure function right return floating_value_mirror is
        begin
            return f_right;
        end function;

        impure function low return floating_value_mirror is
        begin
            return f_low;
        end function;

        impure function high return floating_value_mirror is
        begin
            return f_high;
        end function;

        impure function ascending return boolean is
        begin
            return f_ascending;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type physical_value_mirror_pt is protected body
        impure function get_subtype_mirror return physical_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function unit_index return index is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function value return integer is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function image return string is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type physical_subtype_mirror_pt is protected body
        impure function to_subtype_mirror return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function units_length return index is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function unit_name (unit_idx: index) return string is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function unit_index (unit_name : string) return index is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function scale (unit_idx: index) return natural is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function scale (unit_name: string) return natural is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function simple_name return string is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function left return physical_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function right return physical_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function low return physical_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function high return physical_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function length return index is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function ascending return boolean is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type record_value_mirror_pt is protected body
        impure function get_subtype_mirror return record_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get (element_idx : index) return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get (element_name : string) return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type record_subtype_mirror_pt is protected body
        impure function to_subtype_mirror return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function length return index is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function element_name (element_idx : index) return string is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function element_index (element_name : string) return index is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function element_subtype (element_idx : index) return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function element_subtype (element_name : string) return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function simple_name return string is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type array_value_mirror_pt is protected body
        variable f_owner   : value_mirror;
        variable f_subtype : array_subtype_mirror;

        impure function get_subtype_mirror return array_subtype_mirror is
        begin
            return f_subtype;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            return f_owner;
        end function;

        impure function get (idx : index) return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get (idx1, idx2 : index) return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get (idx1, idx2, idx3 : index) return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get (idx : index_vector) return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type array_subtype_mirror_pt is protected body
        variable f_owner           : subtype_mirror;
        variable f_dimensions      : dimension;
        variable f_element_subtype : subtype_mirror;

        type dimension_rec is record
            f_index_subtype : subtype_mirror;
            f_left          : index;
            f_right         : index;
            f_length        : index;
            f_ascending     : boolean;
        end record;

        type dimension_array is array (dimension range <>) of dimension_rec;
        type dimension_array_ptr is access dimension_array;

        variable f_dimension_data : dimension_array_ptr;

        impure function to_subtype_mirror return subtype_mirror is
        begin
            return f_owner;
        end function;

        impure function dimensions return dimension is
        begin
            return f_dimensions;
        end function;

        impure function index_subtype(idx : dimension := 1) return subtype_mirror is
        begin
            return f_dimension_data.all(idx).f_index_subtype;
        end function;

        impure function element_subtype return subtype_mirror is
        begin
            return f_element_subtype;
        end function;

        impure function simple_name return string is
        begin
            return f_owner.simple_name;
        end function;

        impure function left (idx : dimension := 1) return index is
        begin
            return f_dimension_data.all(idx).f_left;
        end function;

        impure function right (idx : dimension := 1) return index is
        begin
            return f_dimension_data.all(idx).f_right;
        end function;

        impure function low (idx : dimension := 1) return index is
        begin
            return left when ascending else right;
        end function;

        impure function high (idx : dimension := 1) return index is
        begin
            return right when ascending else left;
        end function;

        impure function length (idx : dimension := 1) return index is
        begin
            return f_dimension_data.all(idx).f_length;
        end function;

        impure function ascending (idx : dimension := 1) return boolean is
        begin
            return f_dimension_data.all(idx).f_ascending;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type access_value_mirror_pt is protected body
        impure function get_subtype_mirror return access_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function is_null return boolean is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type access_subtype_mirror_pt is protected body
        impure function to_subtype_mirror return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function simple_name return string is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function designated_subtype return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type file_value_mirror_pt is protected body
        impure function get_subtype_mirror return file_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get_file_logical_name return string is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function get_file_open_kind return file_open_kind is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type file_subtype_mirror_pt is protected body
        impure function to_subtype_mirror return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function simple_name return string is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function designated_subtype return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type protected_value_mirror_pt is protected body
        impure function get_subtype_mirror return protected_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_value_mirror return value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type protected_subtype_mirror_pt is protected body
        impure function to_subtype_mirror return subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function simple_name return string is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type subtype_mirror_pt is protected body
        variable f_class       : type_class;
        variable f_name        : string_ptr;
        variable f_integer     : integer_subtype_mirror;
        variable f_enumeration : enumeration_subtype_mirror;
        variable f_floating    : floating_subtype_mirror;
        variable f_array       : array_subtype_mirror;

        impure function get_type_class return type_class is
        begin
            return f_class;
        end function;

        impure function to_enumeration return enumeration_subtype_mirror is
        begin
            assert f_class = CLASS_ENUMERATION;
            return f_enumeration;
        end function;

        impure function to_integer return integer_subtype_mirror is
        begin
            assert f_class = CLASS_INTEGER;
            return f_integer;
        end function;

        impure function to_floating return floating_subtype_mirror is
        begin
            assert f_class = CLASS_FLOATING;
            return f_floating;
        end function;

        impure function to_physical return physical_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_record return record_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_array return array_subtype_mirror is
        begin
            assert f_class = CLASS_ARRAY;
            return f_array;
        end function;

        impure function to_access return access_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_file return file_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_protected return protected_subtype_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function simple_name return string is
        begin
            return f_name.all;
        end function;
    end protected body;

    ---------------------------------------------------------------------------

    type value_mirror_pt is protected body
        variable f_class       : value_class;
        variable f_subtype     : subtype_mirror;
        variable f_integer     : integer_value_mirror;
        variable f_enumeration : enumeration_value_mirror;
        variable f_floating    : floating_value_mirror;
        variable f_array       : array_value_mirror;

        impure function get_value_class return value_class is
        begin
            return f_class;
        end function;

        impure function get_subtype_mirror return subtype_mirror is
        begin
            return f_subtype;
        end function;

        impure function to_enumeration return enumeration_value_mirror is
        begin
            assert f_class = CLASS_ENUMERATION;
            return f_enumeration;
        end function;

        impure function to_integer return integer_value_mirror is
        begin
            assert f_class = CLASS_INTEGER;
            return f_integer;
        end function;

        impure function to_floating return floating_value_mirror is
        begin
            assert f_class = CLASS_FLOATING;
            return f_floating;
        end function;

        impure function to_physical return physical_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_record return record_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_array return array_value_mirror is
        begin
            assert f_class = CLASS_ARRAY;
            return f_array;
        end function;

        impure function to_access return access_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_file return file_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;

        impure function to_protected return protected_value_mirror is
        begin
            report "unimplemented" severity failure;
        end function;
    end protected body;

end package body;
