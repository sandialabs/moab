from pymoab import core
from pymoab import types
from pymoab.scd import ScdInterface
from pymoab.hcoord import HomCoord
from subprocess import call
import numpy as np
import os

def test_load_mesh():
    mb = core.Core()
    mb.load_file("cyl_grps.h5m")

def test_write_mesh():
    mb = core.Core()
    mb.create_vertices(np.ones(3))
    mb.write_file("outfile.h5m")
    assert os.path.isfile("outfile.h5m")

def test_get_tag():
    mb = core.Core()
    #Create new tag
    tag = mb.tag_get_handle("Test",1,types.MB_TYPE_DOUBLE,True)
    #Create tag with speicified storage type
    tag = mb.tag_get_handle("Test1",1,types.MB_TYPE_DOUBLE,True,types.MB_TAG_SPARSE)
    #Query for exisiting tags
    ret_tag = mb.tag_get_handle("Test")
    ret_tag = mb.tag_get_handle("Test1")
    #Query for unknown tag, without raising exception
    tagtag = mb.tag_get_handle("Fake Tag", exceptions=(types.MB_TAG_NOT_FOUND,))

    #A bunch of invalid tag queries which should fail
    try:
        tag = mb.tag_get_handle("Fake Tag")
    except:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

    try:
        tag = mb.tag_get_handle("Fake Tag",1)
    except:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

    try:
        tag = mb.tag_get_handle("Fake Tag",None,types.MB_TYPE_DOUBLE)
    except:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

def test_integer_tag():
    mb = core.Core()
    vh = vertex_handle(mb)
    test_tag = mb.tag_get_handle("Test",1,types.MB_TYPE_INTEGER,True)
    test_val = 4
    test_tag_data = np.array((test_val,))
    mb.tag_set_data(test_tag, vh, test_tag_data)
    data = mb.tag_get_data(test_tag, vh)

    assert len(data) == 1
    assert data[0] == test_val
    assert data.dtype == 'int32'

def test_double_tag():
    mb = core.Core()
    vh = vertex_handle(mb)
    test_tag = mb.tag_get_handle("Test",1,types.MB_TYPE_DOUBLE,True)
    test_val = 4.4
    test_tag_data = np.array((test_val))
    mb.tag_set_data(test_tag, vh, test_tag_data)
    data = mb.tag_get_data(test_tag, vh)

    assert len(data) == 1
    assert data[0] == test_val
    assert data.dtype == 'float64'

    #a couple of tests that should fail
    test_tag = mb.tag_get_handle("Test1",1,types.MB_TYPE_DOUBLE,True)
    test_val = 4.4
    test_tag_data = np.array((test_val),dtype='float32')
    try:
        mb.tag_set_data(test_tag, vh, test_tag_data)
    except AssertionError:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

    test_tag = mb.tag_get_handle("Test2",1,types.MB_TYPE_DOUBLE,True)
    test_val = 4.4
    test_tag_data = np.array((test_val),dtype='int32')
    try:
        mb.tag_set_data(test_tag, vh, test_tag_data)
    except AssertionError:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

def test_opaque_tag():
    mb = core.Core()
    vh = vertex_handle(mb)
    tag_length = 6
    test_tag = mb.tag_get_handle("Test",tag_length,types.MB_TYPE_OPAQUE,True)
    test_val = 'four'
    test_tag_data = np.array((test_val,))
    mb.tag_set_data(test_tag, vh, test_tag_data)
    data = mb.tag_get_data(test_tag, vh)

    assert len(data) == 1
    assert data.nbytes == tag_length
    assert data[0] == test_val
    assert data.dtype == '|S' + str(tag_length)

    mb.write_file("four.h5m")

def test_create_meshset():
    mb = core.Core()
    msh = mb.create_meshset()
    vh = vertex_handle(mb)
    mb.add_entities(msh,vh)
    
def vertex_handle(core):
    """Convenience function for getting an arbitrary vertex element handle."""
    coord = np.array((1,1,1),dtype='float64')
    vert = core.create_vertices(coord)
    vert_copy = np.array((vert[0],),dtype='uint64')
    return vert_copy

def test_create_elements():
    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    assert 3 == verts.size()
    #create elements
    verts = np.array(((verts[0],verts[1],verts[2]),),dtype='uint64')
    tris = mb.create_elements(types.MBTRI,verts)
    assert 1 == len(tris)
    #check that the element is there via GLOBAL_ID tag
    global_id_tag = mb.tag_get_handle("GLOBAL_ID",1,types.MB_TYPE_INTEGER,True)
    tri_id = mb.tag_get_data(global_id_tag, tris)
    assert 1 == len(tri_id)
    assert 0 == tri_id[0]

def test_range():

    mb = core.Core()
    coord = np.array((1,1,1),dtype='float64')
    vert = mb.create_vertices(coord)
    test_tag = mb.tag_get_handle("Test",1,types.MB_TYPE_INTEGER,True)
    data = np.array((1,))
    mb.tag_set_data(test_tag,vert,data)

    dum = 0
    for v in vert:
        dum += 1
        if dum > 100: break
    assert vert.size() == dum
    assert len(vert) == vert.size()


def test_tag_failures():
    mb = core.Core()
    coord = np.array((1,1,1),dtype='float64')
    verts = mb.create_vertices(coord)
    verts_illicit_copy = np.array((verts[0],),dtype='uint32')
    test_tag = mb.tag_get_handle("Test",1,types.MB_TYPE_INTEGER,True)
    data = np.array((1,))

    #this operation should fail due to the entity handle data type
    mb.tag_set_data(test_tag,verts,data)
    try:
        mb.tag_set_data(test_tag, verts_illicit_copy, data)
    except AssertionError:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError


    global_id_tag = mb.tag_get_handle("GLOBAL_ID",1,types.MB_TYPE_INTEGER,True)
    #so should this one
    try:
        tri_id = mb.tag_get_data(global_id_tag, verts_illicit_copy)
    except AssertionError:
        pass
    else:
        print "Shouldn't be here. Test fails."
        raise AssertionError

def test_adj():

    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    #create elements
    verts = np.array(((verts[0],verts[1],verts[2]),),dtype='uint64')
    tris = mb.create_elements(types.MBTRI,verts)
    #get the adjacencies of the triangle of dim 1 (should return the vertices)
    adjs = mb.get_adjacencies(tris, 0, False)
    assert 3 is adjs.size()

    #check that the entities are of the correct type
    for adj in adjs:
        type = mb.type_from_handle(adj)
        assert type is types.MBVERTEX

    #now get the edges and ask MOAB to create them for us
    adjs = mb.get_adjacencies(tris, 1, True)
    assert 3 is adjs.size()

    for adj in adjs:
        type = mb.type_from_handle(adj)
        assert type is types.MBEDGE

def test_meshsets():
    mb = core.Core()
    parent_set = mb.create_meshset()
    for i in range(5):
        a = mb.create_meshset()
        mb.add_child_meshset(parent_set,a)
    children = mb.get_child_meshsets(parent_set)
    assert children.size() is 5

def test_rs():
    mb = core.Core()
    rs = mb.get_root_set()
    assert rs == 0

def test_get_coords():
    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    ret_coords = mb.get_coords(verts)
    print ret_coords
    for i in range(len(coords)):
            assert coords[i] ==  ret_coords[i]

def test_get_ents_by_type():
    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    ms = mb.create_meshset()
    mb.add_entities(ms,verts)
    ret_verts = mb.get_entities_by_type(ms,types.MBVERTEX, False)
    for i in range(verts.size()):
        assert verts[i] == ret_verts[i]

def test_get_ents_by_tnt():
    
    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    int_test_tag = mb.tag_get_handle("IntegerTestTag",1,types.MB_TYPE_INTEGER,True)
    int_test_tag_values = np.array((0,1,2,))
    mb.tag_set_data(int_test_tag,verts,int_test_tag_values)

    dbl_test_tag = mb.tag_get_handle("DoubleTestTag",1,types.MB_TYPE_DOUBLE,True)
    dbl_test_tag_values = np.array((3.0,4.0,5.0))
    mb.tag_set_data(dbl_test_tag,verts,dbl_test_tag_values)

    opaque_test_tag = mb.tag_get_handle("OpaqueTestTag",5,types.MB_TYPE_OPAQUE,True)
    opaque_test_tag_values = np.array(("Six","Seven","Eight",))
    mb.tag_set_data(opaque_test_tag,verts,opaque_test_tag_values)

    mb.write_file("test.h5m")
    
    rs = mb.get_root_set()

    single_tag_test_cases = []
    ###INTEGER TAG TESTS###
    integer_tag_test_cases = [
        dict(tag_arr = [int_test_tag], value_arr = np.array([[1]]), expected_size = 1),        # existing value
        dict(tag_arr = [int_test_tag], value_arr = np.array([[16]]), expected_size = 0),       # nonexistant value
        dict(tag_arr = [int_test_tag], value_arr = np.array([[None]]), expected_size = 3) ]    # any value
    single_tag_test_cases += integer_tag_test_cases
    ###DOUBLE TAG TESTS###
    double_tag_test_cases = [
        dict(tag_arr = [dbl_test_tag], value_arr = np.array([[4.0]]), expected_size = 1),      # existing value
        dict(tag_arr = [dbl_test_tag], value_arr = np.array([[16.0]]), expected_size = 0),     # nonexistant value
        dict(tag_arr = [dbl_test_tag], value_arr = np.array([[None]]), expected_size = 3) ]    # any value
    single_tag_test_cases += double_tag_test_cases
    ###OPAQUE TAG TESTS###
    opaque_tag_test_cases = [
        dict(tag_arr = [opaque_test_tag], value_arr = np.array([["Six"]]), expected_size = 1), # existing value
        dict(tag_arr = [opaque_test_tag], value_arr = np.array([["Ten"]]), expected_size = 0), # nonexistant value
        dict(tag_arr = [opaque_test_tag], value_arr = np.array([[None]]), expected_size = 3) ] # any value
    single_tag_test_cases += opaque_tag_test_cases
    
    for test_case in single_tag_test_cases:
        entities = mb.get_entities_by_type_and_tag(rs,
                                                   types.MBVERTEX,
                                                   test_case['tag_arr'],
                                                   test_case['value_arr'])
        assert entities.size() == test_case['expected_size']

    ###MIXED TAG TYPE TESTS###
    mixed_tag_test_cases = [
        # existing values
        dict(tag_arr = [int_test_tag,opaque_test_tag],
             value_arr = np.array([[1],["Seven"]], dtype='O'), #dtype must be specified here
             expected_size = 1),
        # any and existing value
        dict(tag_arr = [dbl_test_tag,int_test_tag],
             value_arr = np.array([[None],[1]]),
             expected_size = 1),
        # any and existing value (None comes second)
        dict(tag_arr = [dbl_test_tag,int_test_tag],
             value_arr = np.array([[3.0],[None]]),
             expected_size = 1),
        # any values for both tags
        dict(tag_arr = [dbl_test_tag,int_test_tag],
             value_arr = np.array([[None],[None]]),
             expected_size = 3),
        # mixed types, both nonexisting
        dict(tag_arr = [int_test_tag,dbl_test_tag],
             value_arr = np.array([[6],[10.0]], dtype = 'O'),
             expected_size = 0),
        #mixed types, both existing but not on same entity
        dict(tag_arr = [int_test_tag,dbl_test_tag],
             value_arr = np.array([[1],[2.0]],dtype='O'),
             expected_size = 0),
        #mixed types, only one existing value
        dict(tag_arr = [int_test_tag,dbl_test_tag],
             value_arr = np.array([[1],[12.0]],dtype='O'),
             expected_size = 0)]

    for test_case in mixed_tag_test_cases:
        entities = mb.get_entities_by_type_and_tag(rs,
                                                   types.MBVERTEX,
                                                   test_case['tag_arr'],
                                                   test_case['value_arr'])
        assert entities.size() == test_case['expected_size']

    ###VECTOR TAG TESTS###

    int_vec_test_tag = mb.tag_get_handle("IntegerVecTestTag",3,types.MB_TYPE_INTEGER,True)
    dbl_vec_test_tag = mb.tag_get_handle("DoubleVecTestTag",3,types.MB_TYPE_DOUBLE,True)

    int_vec_test_tag_values = np.array([0,1,2,3,4,5,6,7,8])
    dbl_vec_test_tag_values = np.array([9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0,17.0])
    mb.tag_set_data(int_vec_test_tag,verts,int_vec_test_tag_values)
    mb.tag_set_data(dbl_vec_test_tag,verts,dbl_vec_test_tag_values)

    mb.write_file("vec_tags.h5m")
    # existing sets of values
    entities = mb.get_entities_by_type_and_tag(rs,
                                               types.MBVERTEX,
                                               [int_vec_test_tag,dbl_vec_test_tag],
                                               np.array([[0,1,2],[9.0,10.0,11.0]],dtype='O'))
    assert entities.size() == 1

    #one non existant set of values, one existing
    entities = mb.get_entities_by_type_and_tag(rs,
                                               types.MBVERTEX,
                                               [int_vec_test_tag,dbl_vec_test_tag],
                                               np.array([[0,1,2],[22.0,10.0,11.0]],dtype='O'))

    assert entities.size() == 0
    # any set of one tag
    print "Running suspect test"
    entities = mb.get_entities_by_type_and_tag(rs,
                                               types.MBVERTEX,
                                               [int_vec_test_tag,dbl_vec_test_tag],
                                               np.array([[None,None,None],[9.0,10.0,11.0]]),dtype='O')
    assert entities.size() == 1

    entities = mb.get_entities_by_type_and_tag(rs,
                                               types.MBVERTEX,
                                               [int_vec_test_tag,dbl_vec_test_tag],
                                               np.array([[None],[None]],dtype='O'))
    assert entities.size() == 3
    
        
    # any hex elements tagged with int_test_tag (no hex elements exist, there should be none)
    tag_test_vals = np.array([[None]])        
    entities = mb.get_entities_by_type_and_tag(rs,
                                               types.MBHEX,
                                               np.array((int_test_tag,)),
                                               tag_test_vals)
    print entities.size()
    assert entities.size() == 0

def test_get_entities_by_handle():
    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    ms = mb.create_meshset()
    mb.add_entities(ms,verts)
    ret_verts = mb.get_entities_by_handle(ms, False)
    for i in range(verts.size()):
        assert verts[i] == ret_verts[i]

def test_get_entities_by_dimension():
    mb = core.Core()
    coords = np.array((0,0,0,1,0,0,1,1,1),dtype='float64')
    verts = mb.create_vertices(coords)
    rs = mb.get_root_set()
    ret_verts = mb.get_entities_by_dimension(rs, 0)
    for i in range(verts.size()):
        assert verts[i] == ret_verts[i]

def test_parent_child():
    mb = core.Core()
    
    parent_set = mb.create_meshset()
    child_set = mb.create_meshset()

    mb.add_parent_meshset(child_set, parent_set)

    parent_sets = mb.get_parent_meshsets(child_set)
    assert parent_sets.size() == 1
    assert parent_sets[0] == parent_set

    child_sets = mb.get_child_meshsets(parent_set)
    assert child_sets.size() == 0

    mb.add_child_meshset(parent_set,child_set)
    
    child_sets = mb.get_child_meshsets(parent_set)
    assert child_sets.size() == 1
    assert child_sets[0] == child_set
    
    parent_set = mb.create_meshset()
    child_set = mb.create_meshset()

    parent_sets = mb.get_parent_meshsets(child_set)
    assert parent_sets.size() == 0
    child_sets = mb.get_child_meshsets(parent_set)
    assert child_sets.size() == 0

    mb.add_parent_child(parent_set,child_set)

    parent_sets = mb.get_parent_meshsets(child_set)
    assert parent_sets.size() == 1
    parent_sets[0] == parent_set
    child_sets = mb.get_child_meshsets(parent_set)
    assert child_sets.size() == 1
    child_sets[0] == child_set
    
