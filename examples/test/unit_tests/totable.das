[export]
def test
    let a = [[ auto 1 => "one"; 2 => "two"; 3 => "three" ]]
    let b <- to_table([[ auto 1 => "one"; 2 => "two"; 3 => "three" ]])
    for t in a
        assert(b[t._0]==t._1)
    return true

