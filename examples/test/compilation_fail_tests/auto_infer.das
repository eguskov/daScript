expect 31101, 31102:9, 30304:2, 30105:1, 30102:2, 30106

let
    a:auto                      // 31101, can't infer, need initializer

let
    b = a                       // 31102, can't infer, auto to auto

def blk(b:block<(a:int):int>) : int
    return invoke(b,3)

def test3
    let v1 = b                          // 31102, can't infer, auto to auto
    let v2 : auto& = 13                 // 31102, can't infer, auto& to int (needs to be ref)
    let x = 14  // x:int
    let v3 : auto& = x
    let arr : int[2][3]
    let arr2 : auto[2][3] = arr
    let v4 : auto[3][2] = arr           // 31102, can't infer, array dimension mismatch
    let v5 : auto? = x                  // 31102, can't infer, expecting pointer
    let pT : int?
    let ppT : int? ?
    let ppA : auto? ? = ppT
    let v6 : auto? ? = pT               // 31102, can't infer, expecting pointer to pointer
    blk() <| $(a)
        return a + 1
    blk() <| $(a:int):auto?             // 30304: no matching function (blk block<(int):auto?>)
        return a                        // 30102: incompatible return type, expecting auto? vs int
                                        // 31102: type can't be infered, auto?, returns int
    blk() <| $(a:auto?=3):int           // 30304: no matching function (blk block<(auto?):int>)
                                        // 31102: block argument initialization type can't be infered, auto? = int
        return a                        // 30102: incompatible return type, expecting int vs auto?
    let tab : table<string;int>
    let tabI : table<auto;float> = tab  // 31102: local variable initialization type can't be infered
                                        // 30106: Table key has to be basic 'hashable' type
                                        // 30105: table key is not fully resolved, auto
