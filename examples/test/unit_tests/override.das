struct Foo
    x, y : int = 0
    set = @Foo_setXY
    set2 = @Foo_setXY

def Foo_setXY(var this:Foo;x,y:int)
    this.x = x
    this.y = y

struct Foo3D : Foo
    z : int = 3
    override set = cast<auto> @Foo3D_setXY
    override set2 = @Foo3D_setXY2

def Foo3D_setXY(var this:Foo3D;x,y:int)
    this.x = x
    this.y = y
    this.z = 0

[unsafe]
def Foo3D_setXY2(var that:Foo;x,y:int)
    var this : Foo3D& = upcast<auto> that
    this.x = x
    this.y = y
    this.z = 0

[export]
def test
    var f = Foo()
    f->set(1,2)
    assert(f.x==1 & f.y==2)
    var f3d = Foo3D()
    assert(f3d.x==0 & f3d.y==0 & f3d.z==3)
    f3d->set(1,2)
    assert(f3d.x==1 & f3d.y==2 & f3d.z==0)
    f3d.z = 13
    f3d->set2(3,4)
    assert(f3d.x==3 & f3d.y==4 & f3d.z==0)
    return true
