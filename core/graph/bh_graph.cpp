/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/
#include <bh.h>
#include <list>
#include <queue>
#include <set>
#include <iostream>
#include <fstream>

#include "bh_graph.hpp"

#ifdef __GNUC__
#include <ext/hash_map>
namespace std { using namespace __gnu_cxx; }
#define hashmap std::hash_map
#else
#include <hash_map>
#define hashmap std::hash_map
#endif

struct hash_bh_intp{
  size_t operator()(const bh_intp x) const{
    return (size_t)x;
  }
};
struct hash_bh_graph_node_p{
  size_t operator()(const bh_graph_node* x) const{
    return (size_t)x;
  }
};
struct hash_bh_base_p{
  size_t operator()(const bh_base* x) const{
    return (size_t)x;
  }
};

// Static counter, used to generate sequential filenames when printing multiple batches
static bh_intp print_graph_filename = 0;

/* Returns a pointer to the base array
 * or NULL if the view is NULL
 *
 * @view The view to get the base for
 * @return The base array pointer or NULL
 */
bh_base* bh_get_basearray(bh_view* view)
{
    if(view == NULL)
        return NULL;
    else
        return bh_base_array(view);
}

bh_error bh_graph_getIds(bh_instruction* instr, bh_intp* nops, bh_base** selfId, bh_base** leftId, bh_base** rightId)
{
    *selfId = NULL;
    *leftId = NULL;
    *rightId = NULL;
    *nops = 0;

    bh_view* operands;
    if (instr->opcode != BH_USERFUNC)
    {
        *nops = bh_operands(instr->opcode);
        operands = instr->operand;
    }
    else
    {
        bh_userfunc* uf = instr->userfunc;
        *nops = uf->nout + uf->nin;
        operands = (bh_view*)uf->operand;

        if (uf == NULL || uf->nout != 1 || (uf->nin != 0 && uf->nin != 1 && uf->nin != 2))
        {
            printf("Bailing because the userfunc is weird :(");
            return BH_ERROR;
        }
    }

    *selfId = bh_get_basearray(&operands[0]);
    if (*nops >= 2)
        *leftId = bh_get_basearray(&operands[1]);
    if (*nops >= 3)
        *rightId = bh_get_basearray(&operands[2]);

    return BH_SUCCESS;
}

/* Creates a new graph storage element
 *
 * @bhir A pointer to the result
 * @instructions The initial instruction list (can be NULL if @instruction_count is 0)
 * @instruction_count The number of instructions in the initial list
 * @return BH_ERROR on allocation failure, otherwise BH_SUCCESS
 */
bh_error bh_graph_create(bh_ir** bhir, bh_instruction* instructions, bh_intp instruction_count)
{
    bh_ir* ir = (bh_ir*)malloc(sizeof(bh_ir));
    *bhir = NULL;
    if (ir == NULL)
        return BH_ERROR;

    ir->root = INVALID_NODE;

    ir->nodes = bh_dynamic_list_create(sizeof(bh_graph_node), 4000);
    if (ir->nodes == NULL)
    {
        free(ir);
        return BH_ERROR;
    }
    ir->instructions = bh_dynamic_list_create(sizeof(bh_instruction), 2000);
    if (ir->instructions == NULL)
    {
        bh_dynamic_list_destroy(ir->nodes);
        free(ir);
        return BH_ERROR;
    }

    if (instruction_count > 0)
    {
        if (bh_graph_append(ir, instructions, instruction_count) != BH_SUCCESS)
        {
            bh_dynamic_list_destroy(ir->nodes);
            bh_dynamic_list_destroy(ir->instructions);
            free(ir);
            return BH_ERROR;
        }
    }
    *bhir = ir;
    return BH_SUCCESS;
}

/* Removes all allocated nodes
 *
 * @bhir The graph to update
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_graph_delete_all_nodes(bh_ir* bhir)
{
    while(bhir->nodes->count > 0)
        bh_dynamic_list_remove(bhir->nodes, bhir->nodes->count - 1);
    bhir->root = INVALID_NODE;

    return BH_SUCCESS;
}

/* Cleans up all memory used by the graph
 *
 * @bhir The graph to destroy
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_graph_destroy(bh_ir* bhir)
{
    if (bhir == NULL || bhir->nodes == NULL || bhir->instructions == NULL)
        return BH_ERROR;

    bh_dynamic_list_destroy(bhir->nodes);
    bh_dynamic_list_destroy(bhir->instructions);
    bhir->nodes = NULL;
    bhir->instructions = NULL;
    bhir->root = INVALID_NODE;

    return BH_SUCCESS;
}

/* Appends a new instruction to the current graph
 *
 * @bhir The graph to update
 * @instructions The instructions to append
 * @instruction_count The number of instructions in the list
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_graph_append(bh_ir* bhir, bh_instruction* instructions, bh_intp instruction_count)
{
    if (bhir->root >= 0)
    {
        // Updating is not supported,
        // we need to maintain the read/write maps for that to work
        // but we do not want to copy those structures around
        return BH_ERROR;
    }

    for(bh_intp i = 0; i < instruction_count; i++)
    {
        bh_intp ix = bh_dynamic_list_append(bhir->instructions);
        if (ix < 0)
            return BH_ERROR;
        ((bh_instruction*)bhir->instructions->data)[ix] = instructions[i];
    }

    return BH_SUCCESS;
}


/* Creates a new graph node.
 *
 * @type The node type
 * @instruction The instruction attached to the node, or NULL
 * @return Error codes (BH_SUCCESS)
 */
bh_node_index bh_graph_new_node(bh_ir* bhir, bh_intp type, bh_instruction_index instruction)
{
    bh_node_index ix = (bh_node_index)bh_dynamic_list_append(bhir->nodes);
    if (ix < 0)
        return INVALID_NODE;

    NODE_LOOKUP(ix).type = type;
    NODE_LOOKUP(ix).instruction = instruction;
    NODE_LOOKUP(ix).left_child = INVALID_NODE;
    NODE_LOOKUP(ix).right_child = INVALID_NODE;
    NODE_LOOKUP(ix).left_parent = INVALID_NODE;
    NODE_LOOKUP(ix).right_parent = INVALID_NODE;

    return ix;
}

/* Destroys a new graph node.
 *
 * @bhir The bh_ir structure
 * @node The node to free
 */
void bh_graph_free_node(bh_ir* bhir, bh_node_index node)
{
    bh_dynamic_list_remove(bhir->nodes, node);
}

/* Parses the instruction list and creates a new graph
 *
 * @bhir The graph to update
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_graph_parse(bh_ir* bhir)
{
    hashmap<bh_base*, bh_node_index, hash_bh_base_p> writemap;
    hashmap<bh_base*, std::set<bh_node_index>*, hash_bh_base_p> readmap;
    hashmap<bh_base*, bh_node_index, hash_bh_base_p>::iterator write_it;
    hashmap<bh_base*, std::set<bh_node_index>*, hash_bh_base_p>::iterator read_it;

    // If already parsed, just return
    if (bhir->root >= 0)
        return BH_SUCCESS;

    print_graph_filename++;

    if (getenv("BH_PRINT_INSTRUCTION_GRAPH") != NULL)
    {
        //Debug case only!
        char filename[8000];

        snprintf(filename, 8000, "%sinstlist-%lld.dot", getenv("BH_PRINT_INSTRUCTION_GRAPH"), (long long)print_graph_filename);
        bh_graph_print_from_instructions(bhir, filename);
    }

#ifdef DEBUG
    bh_intp instrCount = 0;
#endif

    bh_intp root = bh_graph_new_node(bhir, BH_COLLECTION, INVALID_INSTRUCTION);

    for(bh_intp i = 0; i < bhir->instructions->count; i++)
    {
        // We can keep a reference pointer as we do not need to update the list
        // while traversing it
        bh_instruction* instr = &(((bh_instruction*)bhir->instructions->data)[i]);

        bh_base* selfId;
        bh_base* leftId;
        bh_base* rightId;
        bh_intp nops;

        if (bh_graph_getIds(instr, &nops, &selfId, &leftId, &rightId) != BH_SUCCESS)
        {
            bh_graph_delete_all_nodes(bhir);
            return BH_ERROR;
        }

        bh_node_index selfNode = bh_graph_new_node(bhir, BH_INSTRUCTION, i);
        if (selfNode == INVALID_NODE)
        {
            bh_graph_delete_all_nodes(bhir);
            return BH_ERROR;
        }

        write_it = writemap.find(selfId);
        if (write_it != writemap.end())
        {
            bh_node_index oldTarget = write_it->second;
            if (bh_grap_node_add_child(bhir, oldTarget, selfNode) != BH_SUCCESS)
            {
                bh_graph_delete_all_nodes(bhir);
                return BH_ERROR;
            }
        }

        writemap[selfId] = selfNode;

        bh_node_index leftDep = INVALID_NODE;
        bh_node_index rightDep = INVALID_NODE;
        write_it = writemap.find(leftId);
        if (write_it != writemap.end())
            leftDep = write_it->second;
        write_it = writemap.find(rightId);
        if (write_it != writemap.end())
            rightDep = write_it->second;

        read_it = readmap.find(selfId);
        if (read_it != readmap.end())
        {
            for(std::set<bh_node_index>::iterator it = read_it->second->begin(); it != read_it->second->end(); ++it)
            {
                if (*it != leftDep && *it != rightDep)
                {
                    if (bh_grap_node_add_child(bhir, *it, selfNode) != BH_SUCCESS)
                    {
                        bh_graph_delete_all_nodes(bhir);
                        return BH_ERROR;
                    }
                }
            }
            delete read_it->second;
            readmap.erase(read_it);
        }

        if (leftId != NULL)
        {
            std::set<bh_node_index>* nodes;

            read_it = readmap.find(leftId);
            if (read_it == readmap.end())
            {
                nodes = new std::set<bh_node_index>();
                readmap[leftId] = nodes;
            }
            else
                nodes = read_it->second;

            nodes->insert(selfNode);
        }

        if (rightId != NULL && rightId != leftId)
        {
            std::set<bh_node_index>* nodes;

            read_it = readmap.find(rightId);
            if (read_it == readmap.end())
            {
                nodes = new std::set<bh_node_index>();
                readmap[rightId] = nodes;
            }
            else
                nodes = read_it->second;

            nodes->insert(selfNode);
        }

        if (leftDep != INVALID_NODE && leftDep != selfNode)
        {
            if (bh_grap_node_add_child(bhir, leftDep, selfNode) != BH_SUCCESS)
            {
                bh_graph_delete_all_nodes(bhir);
                return BH_ERROR;
            }
        }

        if (rightDep != INVALID_NODE && rightDep != leftDep && rightDep != selfNode)
        {
            if (bh_grap_node_add_child(bhir, rightDep, selfNode) != BH_SUCCESS)
            {
                bh_graph_delete_all_nodes(bhir);
                return BH_ERROR;
            }
        }

        if (NODE_LOOKUP(selfNode).left_parent == INVALID_NODE && NODE_LOOKUP(selfNode).right_parent)
        {
            if (bh_grap_node_add_child(bhir, root, selfNode) != BH_SUCCESS)
            {
                bh_graph_delete_all_nodes(bhir);
                return BH_ERROR;
            }
        }
    }

    // Clean up dynamically allocated memory
    read_it = readmap.begin();
    while(read_it != readmap.end())
    {
        delete read_it->second;
        ++read_it;
    }

    bhir->root = root;

    if (getenv("BH_PRINT_NODE_INPUT_GRAPH") != NULL)
    {
        //Debug case only!
        char filename[8000];

        snprintf(filename, 8000, "%sinput-graph-%lld.dot", getenv("BH_PRINT_NODE_INPUT_GRAPH"), (long long)print_graph_filename);
        bh_graph_print_graph(bhir, filename);
    }

    return BH_SUCCESS;
}

struct bh_graph_iterator {
    // Keep track of already scheduled nodes
    hashmap<bh_node_index, bh_node_index, hash_bh_intp>* scheduled;
    // Keep track of items that have unsatisfied dependencies
    std::list<bh_node_index>* blocked;
    // The graph we are iterating
    bh_ir* bhir;
    // The currently visited node
    bh_node_index current;
    // The last unprocessed node
    bh_node_index last_blocked;
};

/* Creates a new iterator for visiting nodes in the graph
 *
 * @bhir The graph to iterate
 * @iterator The new iterator
 * @return BH_SUCCESS if the iterator is create, BH_ERROR otherwise
 */
bh_error bh_graph_iterator_create(bh_ir* bhir, bh_graph_iterator** iterator)
{
    if (getenv("BH_PRINT_NODE_OUTPUT_GRAPH") != NULL)
    {
        //Debug case only!
        char filename[8000];

        snprintf(filename, 8000, "%soutput-graph-%lld.dot", getenv("BH_PRINT_NODE_OUTPUT_GRAPH"), (long long)print_graph_filename);
        bh_graph_print_graph(bhir, filename);
    }

    struct bh_graph_iterator* t = (struct bh_graph_iterator*)malloc(sizeof(struct bh_graph_iterator));
    if (t == NULL)
    {
        *iterator = NULL;
        return BH_ERROR;
    }

    // Make sure we have the graph parsed
    if (bhir->root < 0 && getenv("BH_DISABLE_BHIR_GRAPH") == NULL)
    {
        if (bh_graph_parse(bhir) != BH_SUCCESS)
        {
            free(t);
            *iterator = NULL;
            return BH_ERROR;
        }
    }

    t->scheduled = new hashmap<bh_node_index, bh_node_index, hash_bh_intp>();
    t->blocked = new std::list<bh_node_index>();
    t->bhir = bhir;
    t->last_blocked = INVALID_NODE;
    t->current = t->bhir->root;
    if (t->current != INVALID_NODE)
        t->blocked->push_back(t->current);

    *iterator = t;
    return BH_SUCCESS;
}

/* Resets a graph iterator
 *
 * @iterator The iterator to reset
 * @return BH_SUCCESS if the iterator is reset, BH_ERROR otherwise
 */
bh_error bh_graph_iterator_reset(bh_graph_iterator* iterator)
{
    delete iterator->scheduled;
    delete iterator->blocked;

    iterator->scheduled = new hashmap<bh_node_index, bh_node_index, hash_bh_intp>();
    iterator->blocked = new std::list<bh_node_index>();
    iterator->current = iterator->bhir->root;
    if (iterator->current != INVALID_NODE)
        iterator->blocked->push_back(iterator->current);

    return BH_SUCCESS;
}

/* Move a graph iterator
 *
 * @iterator The iterator to move
 * @instruction The next instruction
 * @return BH_SUCCESS if the iterator moved, BH_ERROR otherwise
 */
bh_error bh_graph_iterator_next_instruction(bh_graph_iterator* iterator, bh_instruction** instruction)
{
    bh_ir* bhir = iterator->bhir;

    // If we have not parsed, just give the instruction list as-is
    if (iterator->bhir->root == INVALID_NODE)
    {
        if (iterator->current == INVALID_NODE)
            iterator->current = -1;
        iterator->current++;

        if (iterator->current < bhir->instructions->count)
        {
            *instruction = &INSTRUCTION_LOOKUP(iterator->current);
            return BH_SUCCESS;
        }
        else
        {
            *instruction = NULL;
            return BH_ERROR;
        }
    }

    bh_node_index ix;
    while(bh_graph_iterator_next_node(iterator, &ix) == BH_SUCCESS)
        if (NODE_LOOKUP(ix).type == BH_INSTRUCTION)
        {
            *instruction = &(INSTRUCTION_LOOKUP(NODE_LOOKUP(ix).instruction));
            return BH_SUCCESS;
        }

    *instruction = NULL;
    return BH_ERROR;
}

/* Move a graph iterator
 *
 * @iterator The iterator to move
 * @instruction The next instruction
 * @return BH_SUCCESS if the iterator moved, BH_ERROR otherwise
 */
bh_error bh_graph_iterator_next_node(bh_graph_iterator* iterator, bh_node_index* node)
{
    bh_ir* bhir = iterator->bhir;

    while (!iterator->blocked->empty())
    {
        bh_node_index n = iterator->blocked->front();
        iterator->blocked->pop_front();
        if (n != INVALID_NODE && iterator->scheduled->find(n) == iterator->scheduled->end())
        {
            // Check if dependencies are met
            if ((NODE_LOOKUP(n).left_parent == INVALID_NODE || iterator->scheduled->find(NODE_LOOKUP(n).left_parent) != iterator->scheduled->end()) && (NODE_LOOKUP(n).right_parent == INVALID_NODE || iterator->scheduled->find(NODE_LOOKUP(n).right_parent) != iterator->scheduled->end()))
            {
                iterator->last_blocked = INVALID_NODE;
                (*(iterator->scheduled))[n] = n;

                //Examine child nodes
                if (NODE_LOOKUP(n).left_child != INVALID_NODE)
                    iterator->blocked->push_front(NODE_LOOKUP(n).left_child);
                if (NODE_LOOKUP(n).right_child != INVALID_NODE && NODE_LOOKUP(n).right_child != NODE_LOOKUP(n).left_child)
                    iterator->blocked->push_back(NODE_LOOKUP(n).right_child);

                *node = n;
                return BH_SUCCESS;
            }
            else
            {
                // Re-insert at bottom of work queue
                iterator->blocked->push_back(n);
                if (iterator->last_blocked == n)
                {
                    printf("Invalid graph detected, contains circular dependencies, listing offending node\n");

                    while (!iterator->blocked->empty())
                    {
                        n = iterator->blocked->front();
                        iterator->blocked->pop_front();

                        printf("%s: self: %lld, left_parent: %lld, right_parent: %lld, left_child: %lld, right_child: %lld\n",
                                NODE_LOOKUP(n).type == BH_INSTRUCTION ? bh_opcode_text(INSTRUCTION_LOOKUP(NODE_LOOKUP(n).instruction).opcode) : "BH_COLLECTION",
                                (long long)n,
                                (long long)NODE_LOOKUP(n).left_parent,
                                (long long)NODE_LOOKUP(n).right_parent,
                                (long long)NODE_LOOKUP(n).left_child,
                                (long long)NODE_LOOKUP(n).right_child);

                        if (NODE_LOOKUP(n).type == BH_INSTRUCTION)
                            printf("INSTRUCTION %lld: %s -> %lld\n",
                                    (long long)NODE_LOOKUP(n).instruction,
                                    bh_opcode_text(INSTRUCTION_LOOKUP(NODE_LOOKUP(n).instruction).opcode),
                                    (long long)&INSTRUCTION_LOOKUP(NODE_LOOKUP(n).instruction).operand[0]);

                    }

                    return BH_ERROR;
                }

                if (iterator->last_blocked == INVALID_NODE)
                    iterator->last_blocked = n;
            }
        }
    }

    *node = INVALID_NODE;
    return BH_ERROR;
}

/* Destroys a graph iterator
 *
 * @iterator The iterator to destroy
 * @return BH_SUCCESS if the iterator is destroyed, BH_ERROR otherwise
 */
bh_error bh_graph_iterator_destroy(bh_graph_iterator* iterator)
{
    delete iterator->scheduled;
    delete iterator->blocked;
    iterator->scheduled = NULL;
    iterator->blocked = NULL;
    iterator->bhir = NULL;
    iterator->current = INVALID_NODE;
    free(iterator);

    return BH_SUCCESS;
}

/* Creates a list of instructions from a graph representation.
 *
 * @bhir The graph to serialize
 * @instructions Storage for retrieving the instructions
 * @instruction_count Input the size of the list, outputs the number of instructions
 * @return BH_SUCCESS if all nodes are added to the list, BH_ERROR if the storage was insufficient
 */
bh_error bh_graph_serialize(bh_ir* bhir, bh_instruction* instructions, bh_intp* instruction_count)
{
    bh_graph_iterator* it;
    bh_instruction* dummy;
    bh_intp count = 0;

    if (bh_graph_iterator_create(bhir, &it) != BH_SUCCESS)
        return BH_ERROR;

    while(bh_graph_iterator_next_instruction(it, &dummy) == BH_SUCCESS)
    {
        if (count < *instruction_count)
            instructions[count] = *dummy; //Copy

        count++;
    }

    if (count > *instruction_count)
    {
        *instruction_count = count;
        return BH_ERROR;
    }
    else
    {
        *instruction_count = count;
        return BH_SUCCESS;
    }

    bh_graph_iterator_destroy(it);
}

/* Inserts a node into the graph
 *
 * @bhir The graph to update
 * @self The node to insert before
 * @other The node to insert
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_grap_node_insert_before(bh_ir* bhir, bh_node_index self, bh_node_index other)
{
    NODE_LOOKUP(self).left_child = other;
    if (NODE_LOOKUP(other).left_parent != INVALID_NODE)
    {
        if (NODE_LOOKUP(NODE_LOOKUP(other).left_parent).left_child == other)
            NODE_LOOKUP(NODE_LOOKUP(other).left_parent).left_child = self;
        else if (NODE_LOOKUP(NODE_LOOKUP(other).left_parent).right_child == other)
            NODE_LOOKUP(NODE_LOOKUP(other).left_parent).right_child = self;
        else
        {
            printf("Bad graph");
            return BH_ERROR;
        }

        NODE_LOOKUP(self).left_parent = NODE_LOOKUP(other).left_parent;
    }

    if (NODE_LOOKUP(other).right_parent != INVALID_NODE)
    {
        if (NODE_LOOKUP(NODE_LOOKUP(other).right_parent).left_child == other)
            NODE_LOOKUP(NODE_LOOKUP(other).right_parent).left_child = self;
        else if (NODE_LOOKUP(NODE_LOOKUP(other).right_parent).right_child == other)
            NODE_LOOKUP(NODE_LOOKUP(other).right_parent).right_child = self;
        else
        {
            printf("Bad graph");
            return BH_ERROR;
        }

        NODE_LOOKUP(self).right_parent = NODE_LOOKUP(other).right_parent;
    }

    NODE_LOOKUP(other).left_parent = self;
    NODE_LOOKUP(other).right_parent = INVALID_NODE;

    return BH_SUCCESS;
}

/* Appends a node onto another node in the graph
 *
 * @bhir The graph to update
 * @self The node to append to
 * @newchild The node to append
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_grap_node_add_child(bh_ir* bhir, bh_node_index self, bh_node_index newchild)
{
    if (self == newchild)
    {
        printf("Self appending");
        return BH_ERROR;
    }

    if (NODE_LOOKUP(self).left_child == INVALID_NODE)
    {
        NODE_LOOKUP(self).left_child = newchild;
        bh_grap_node_add_parent(bhir, newchild, self);
    }
    else if (NODE_LOOKUP(self).right_child == INVALID_NODE)
    {
        NODE_LOOKUP(self).right_child = newchild;
        bh_grap_node_add_parent(bhir, newchild, self);
    }
    else
    {
        bh_node_index cn = bh_graph_new_node(bhir, BH_COLLECTION, INVALID_INSTRUCTION);
        if (cn == INVALID_NODE)
            return BH_ERROR;
        NODE_LOOKUP(cn).left_child = NODE_LOOKUP(self).left_child;
        NODE_LOOKUP(cn).right_child = newchild;
        NODE_LOOKUP(self).left_child = cn;

        if (NODE_LOOKUP(NODE_LOOKUP(cn).left_child).left_parent == self)
            NODE_LOOKUP(NODE_LOOKUP(cn).left_child).left_parent = cn;
        else if (NODE_LOOKUP(NODE_LOOKUP(cn).left_child).right_parent == self)
            NODE_LOOKUP(NODE_LOOKUP(cn).left_child).right_parent = cn;
        else
        {
            printf("Bad graph");
            return BH_ERROR;
        }

        if (bh_grap_node_add_parent(bhir, newchild, cn) != BH_SUCCESS)
            return BH_ERROR;

        NODE_LOOKUP(cn).left_parent = self;
    }

    return BH_SUCCESS;
}

/* Inserts a node into the graph
 *
 * @bhir The graph to update
 * @self The node to update
 * @newparent The node to append
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_grap_node_add_parent(bh_ir* bhir, bh_node_index self, bh_node_index newparent)
{
    if (NODE_LOOKUP(self).left_parent == newparent || NODE_LOOKUP(self).right_parent == newparent || newparent == INVALID_NODE)
        return BH_SUCCESS;
    else if (NODE_LOOKUP(self).left_parent == INVALID_NODE)
        NODE_LOOKUP(self).left_parent = newparent;
    else if (NODE_LOOKUP(self).right_parent == INVALID_NODE)
        NODE_LOOKUP(self).right_parent = newparent;
    else
    {
        bh_node_index cn = bh_graph_new_node(bhir, BH_COLLECTION, INVALID_INSTRUCTION);
        if (cn == INVALID_NODE)
            return BH_ERROR;

        NODE_LOOKUP(cn).left_parent = NODE_LOOKUP(self).left_parent;
        NODE_LOOKUP(cn).right_parent = NODE_LOOKUP(self).right_parent;

        if (NODE_LOOKUP(NODE_LOOKUP(self).left_parent).left_child == self)
            NODE_LOOKUP(NODE_LOOKUP(cn).left_parent).left_child = cn;
        else if (NODE_LOOKUP(NODE_LOOKUP(self).left_parent).right_child == self)
            NODE_LOOKUP(NODE_LOOKUP(cn).left_parent).right_child = cn;

        if (NODE_LOOKUP(NODE_LOOKUP(self).right_parent).left_child == self)
            NODE_LOOKUP(NODE_LOOKUP(cn).right_parent).left_child = cn;
        else if (NODE_LOOKUP(NODE_LOOKUP(self).right_parent).right_child == self)
            NODE_LOOKUP(NODE_LOOKUP(cn).right_parent).right_child = cn;

        NODE_LOOKUP(self).left_parent = cn;
        NODE_LOOKUP(self).right_parent = newparent;
        NODE_LOOKUP(cn).left_child = self;
    }

    return BH_SUCCESS;
}

/* Uses the instruction list to calculate dependencies and print a graph in DOT format.
 *
 * @bhir The input instructions
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_graph_print_from_instructions(bh_ir* bhir, const char* filename)
{
    hashmap<bh_base*, bh_intp, hash_bh_base_p> nameDict;
    hashmap<bh_base*, bh_intp, hash_bh_base_p>::iterator it;

    bh_intp lastName = 0;
    bh_intp constName = 0;

    std::ofstream fs(filename);

    fs << "digraph {" << std::endl;

    for(bh_intp i = 0; i < bhir->instructions->count; i++)
    {
        bh_instruction* n = &INSTRUCTION_LOOKUP(i);

        if (n->opcode != BH_USERFUNC)
        {
            bh_base* baseID;
            bh_base* leftID;
            bh_base* rightID;
            bh_intp nops;

            if (bh_graph_getIds(n, &nops, &baseID, &leftID, &rightID) != BH_SUCCESS)
                continue;

            bh_intp parentName;
            bh_intp leftName;
            bh_intp rightName;

            it = nameDict.find(baseID);
            if (it == nameDict.end())
            {
                parentName = lastName++;
                nameDict[baseID] = parentName;
            }
            else
                parentName = it->second;

            it = nameDict.find(leftID);
            if (it == nameDict.end())
            {
                leftName = lastName++;
                nameDict[leftID] = leftName;
            }
            else
                leftName = it->second;

            it = nameDict.find(rightID);
            if (it == nameDict.end())
            {
                rightName = lastName++;
                nameDict[rightID] = rightName;
            }
            else
                rightName = it->second;

            bh_view* operands;
            if (n->opcode != BH_USERFUNC)
                operands = n->operand;
            else
                operands = (bh_view*)n->userfunc->operand;

            if (nops >= 2)
            {
                if (leftID == NULL)
                {
                    bh_intp constid = constName++;
                    fs << "const_" << constid << "[shape=pentagon, style=filled, fillcolor=\"#ff0000\", label=\"" << n->constant.value.float64 << "\"];" << std::endl;
                    fs << "const_" << constid << " -> " << "I_" << i << ";" << std::endl;
                }
                else
                {
                    fs << "B_" << leftName << "[shape=ellipse, style=filled, fillcolor=\"#0000ff\", label=\"B_" << leftName << " - " << bh_get_basearray(&operands[1]) << "\"];" << std::endl;
                    fs << "B_" << leftName << " -> " << "I_" << i << ";" << std::endl;
                }

                if (nops >= 3)
                {
                    if (rightID == NULL)
                    {
                        bh_intp constid = constName++;
                        fs << "const_" << constid << "[shape=pentagon, style=filled, fillcolor=\"#ff0000\", label=\"" << n->constant.value.float64 << "\"];" << std::endl;
                        fs << "const_" << constid << " -> " << "I_" << i << ";" << std::endl;
                    }
                    else
                    {
                        fs << "B_" << rightName << "[shape=ellipse, style=filled, fillcolor=\"#0000ff\", label=\"B_" << rightName << " - " << bh_get_basearray(&operands[2]) << "\"];" << std::endl;
                        fs << "B_" << rightName << " -> " << "I_" << i << ";" << std::endl;
                    }
                }

            }

            fs << "I_" << i << "[shape=box, style=filled, fillcolor=\"#CBD5E8\", label=\"I_" << i << " - " << bh_opcode_text(n->opcode) << "\"];" << std::endl;
            fs << "B_" << parentName << "[shape=ellipse, style=filled, fillcolor=\"#0000ff\", label=\"" << "B_" << parentName << " - " << bh_get_basearray(&operands[0]) << "\"];" << std::endl;

            fs << "I_" << i << " -> " << "B_" << parentName << ";" << std::endl;
        }
    }

    fs << "}" << std::endl;
    fs.close();

    return BH_SUCCESS;
}

/* Prints a graph representation of the node in DOT format.
 *
 * @bhir The graph to print
 * @return Error codes (BH_SUCCESS)
 */
bh_error bh_graph_print_graph(bh_ir* bhir, const char* filename)
{
    std::ofstream fs(filename);

    fs << "digraph {" << std::endl;

    for(bh_node_index node = 0; node < bhir->nodes->count; node++)
    {
        if (node == INVALID_NODE)
            continue;

        const char T = NODE_LOOKUP(node).type == BH_INSTRUCTION ? 'I' : 'C';

        if (NODE_LOOKUP(node).type == BH_INSTRUCTION)
        {
            bh_intp opcode = INSTRUCTION_LOOKUP(NODE_LOOKUP(node).instruction).opcode;
            const char* color = "#CBD5E8"; // = roots.Contains(node) ? "#CBffff" : "#CBD5E8";
            const char* style = (opcode == BH_DISCARD || opcode == BH_FREE) ? "dashed,rounded" : "filled,rounded";
            const char* opcodename;
            if (opcode == BH_DISCARD)
            {
                if (INSTRUCTION_LOOKUP(NODE_LOOKUP(node).instruction).operand[0].base == NULL)
                    opcodename = "BH_BASE_DISCARD";
                else
                    opcodename = "BH_VIEW_DISCARD";
            }
            else
                opcodename = bh_opcode_text(INSTRUCTION_LOOKUP(NODE_LOOKUP(node).instruction).opcode);

            fs << T << "_" << node << " [shape=box style=\"" << style << "\" fillcolor=\"" << color << "\" label=\"" << T << "_" << node << " - " << opcodename << "\"];" << std::endl;
        }
        else if (NODE_LOOKUP(node).type == BH_COLLECTION)
        {
            fs << T << "_" << node << " [shape=box, style=filled, fillcolor=\"#ffffE8\", label=\"" << T << node << " - COLLECTION\"];" << std::endl;
        }

        if (NODE_LOOKUP(node).left_child != INVALID_NODE)
        {
            const char T2 = NODE_LOOKUP(NODE_LOOKUP(node).left_child).type == BH_INSTRUCTION ? 'I' : 'C';
            fs << T << "_" << node << " -> " << T2 << "_" << NODE_LOOKUP(node).left_child << ";" << std::endl;
        }
        if (NODE_LOOKUP(node).right_child != INVALID_NODE)
        {
            const char T2 = NODE_LOOKUP(NODE_LOOKUP(node).right_child).type == BH_INSTRUCTION ? 'I' : 'C';
            fs << T << "_" << node << " -> " << T2 << "_" << NODE_LOOKUP(node).right_child << ";" << std::endl;
        }
    }

    fs << "}" << std::endl;
    fs.close();

    return BH_SUCCESS;
}