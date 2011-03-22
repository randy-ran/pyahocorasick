/*
	Implementation Automaton class methods
	and other class related stuff.

TRIE		= 0
AHOCORASICK	= 1

STORE_STRINGS	= 0
STORE_INTS		= 1
STORE_ANY		= 2

class AutomatonException(Exception):
	pass

class Automaton:
	def __init__(self, store)
		"store in STORE_STRINGS, STORE_INTS, STORE_ANY"
		pass

	def add_word(self, word, value)
		"adds new word to dictionary"
		pass

	def clear(self):
		"removes all words"
		pass
	
	def match(self, word):
		"returns true if word is present in a dictionary"
		pass

	def match_prefix(self, word):
		"""
		Returns true if word is present in a dictionary,
		even if word has no associacted value.
		"""
		pass

	def get(self, word, def):
		"Returns object associated with word."
		pass

	def make_automaton(self):
		"Constuct Aho-Corsick automaton."
		pass

	def search_all(self, string, callback):
		"""
		Callback is called on every occurence of any word from dictionary
		Callback must accept two arguments: position and a tuple
		"""
		pass

*/

static PyObject*
automaton_new(PyObject* self, PyObject* args) {
	Automaton* automaton;
	int store;

	if (PyArg_ParseTuple(args, "i", &store)) {
		switch (store) {
			case STORE_STRINGS:
			case STORE_INTS:
			case STORE_ANY:
				// ok
				break;

			default:
				PyErr_SetString(
					PyExc_ValueError,
					"store must have value STORE_STRINGS, STORE_INTS or STORE_ANY"
				);
				return NULL;
		} // switch
	}
	else {
		PyErr_Clear();
		store = STORE_ANY;
	}
	
	automaton = (Automaton*)PyObject_New(Automaton, &automaton_type);
	if (automaton == NULL)
		return NULL;

	automaton->count = 0;
	automaton->kind  = EMPTY;
	automaton->store = store;
	automaton->root  = NULL;

	return (PyObject*)automaton;
}

static PyObject*
automaton_clear(PyObject* self, PyObject* args);

static void
automaton_del(PyObject* self) {
#define automaton ((Automaton*)self)
	automaton_clear(self, NULL);
	PyObject_Del(self);
#undef automaton
}


#define automaton_len_doc \
	"returns count of words"

ssize_t
automaton_len(PyObject* self) {
#define automaton ((Automaton*)self)
	return automaton->count;
#undef automaton
}


#define automaton_add_word_doc \
	"add new word to dictionary"

static PyObject*
automaton_add_word(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	// argument
	PyObject* py_word;
	PyObject* py_value;

	ssize_t wordlen;
	char* word;
	bool unicode;

	py_word = pymod_get_string(args, 0, &word, &wordlen, &unicode);
	if (not py_word)
		return NULL;

	py_value = PyTuple_GetItem(args, 1);
	if (not py_value)
		return NULL;

	if (wordlen > 0) {
		bool new_word = false;
		TrieNode* node;
		if (unicode)
#ifndef Py_UNICODE_WIDE
			node = trie_add_word_UCS2(automaton, (uint16_t*)word, wordlen, &new_word);
#else
			node = trie_add_word_UCS4(automaton, (uint32_t*)word, wordlen, &new_word);
#endif
		else
			node = trie_add_word(automaton, word, wordlen, &new_word);

		Py_DECREF(py_word);
		if (node) {
			if (new_word) {
				if (node->output)
					Py_DECREF(node->output);

				Py_INCREF(py_value);
				node->output = py_value;
				Py_RETURN_TRUE;
			}
			else {
				Py_INCREF(py_value);
				node->output = py_value;
				Py_RETURN_FALSE;
			}
		}
		else
			return NULL;
	}

	Py_DECREF(py_word);
	Py_RETURN_FALSE;
}


static void
clear_aux(TrieNode* node, KeysStore store) {
	if (node) {
		switch (store) {
			case STORE_INTS:
				// nop
				break;

			case STORE_STRINGS:
				memfree(node->output);
				break;

			case STORE_ANY:
				if (node->output)
					Py_DECREF(node->output);
				break;
		}

		const int n = node->n;
		int i;
		for (i=0; i < n; i++) {
			TrieNode* child = node->next[i];
			if (child != node) // avoid loops!
				clear_aux(child, store);
		}

		memfree(node);
	}
#undef automaton
}


#define automaton_clear_doc\
	"removes all objects from dictionary"


static PyObject*
automaton_clear(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	clear_aux(automaton->root, automaton->store);
	automaton->count = 0;
	automaton->kind = EMPTY;
	automaton->root = NULL;

	Py_RETURN_NONE;
#undef automaton
}


static int
automaton_contains(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	ssize_t wordlen;
	char* word;
	bool unicode;
	PyObject* py_word;

	py_word = pymod_get_string(args, 0, &word, &wordlen, &unicode);
	if (py_word == NULL)
		return -1;

	TrieNode* node;
	if (unicode)
#ifndef Py_UNICODE_WIDE
		node = trie_find_UCS2(automaton->root, (uint16_t*)word, wordlen);
#else
		node = trie_find_UCS4(automaton->root, (uint32_t*)word, wordlen);
#endif
	else
		node = trie_find(automaton->root, word, wordlen);

	Py_DECREF(py_word);
	return (node and node->eow);
#undef automaton
}


#define automaton_match_doc \
	"match(word) => bool - returns if word is in dictionary"

static PyObject*
automaton_match(PyObject* self, PyObject* args) {
	switch (automaton_contains(self, args)) {
		case 1:
			Py_RETURN_TRUE;

		case 0:
			Py_RETURN_FALSE;

		default:
			return NULL;
	}
}


#define automaton_match_prefix_doc \
	"match_prefix(word) => bool - returns if there is a prefix equal to word"

static PyObject*
automaton_match_prefix(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	ssize_t wordlen;
	char* word;
	bool unicode;
	PyObject* py_word;

	py_word = pymod_get_string(args, 0, &word, &wordlen, &unicode);
	if (py_word == NULL)
		return NULL;

	TrieNode* node;
	if (unicode)
#ifndef Py_UNICODE_WIDE
		node = trie_find_UCS2(automaton->root, (uint16_t*)word, wordlen);
#else
		node = trie_find_UCS4(automaton->root, (uint32_t*)word, wordlen);
#endif
	else
		node = trie_find(automaton->root, word, wordlen);
	
	if (node)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
#undef automaton
}


#define automaton_get_doc \
	"get(word, [def]) => obj - returns object associated with given word; " \
	"if word isn't present, then def is returned, when def isn't defined, " \
	"raise KeyError exception"

static PyObject*
automaton_get(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	ssize_t wordlen;
	char* word;
	bool unicode;
	PyObject* py_word;
	PyObject* py_def;

	py_word = pymod_get_string(args, 0, &word, &wordlen, &unicode);
	if (py_word == NULL)
		return NULL;

	TrieNode* node;
	if (unicode)
#ifndef Py_UNICODE_WIDE
		node = trie_find_UCS2(automaton->root, (uint16_t*)word, wordlen);
#else
		node = trie_find_UCS4(automaton->root, (uint32_t*)word, wordlen);
#endif
	else
		node = trie_find(automaton->root, word, wordlen);

	if (node and node->eow) {
		switch (automaton->store) {
			case STORE_STRINGS:
				return Py_BuildValue("s", node->output);

			case STORE_INTS:
				return Py_BuildValue("i", (int)(node->output));

			case STORE_ANY:
				Py_INCREF(node->output);
				return node->output;

			default:
				PyErr_SetNone(PyExc_ValueError);
				return NULL;
		}
	}
	else {
		py_def = PyTuple_GetItem(args, 1);
		if (py_def) {
			Py_INCREF(py_def);
			return py_def;
		}
		else {
			PyErr_Clear();
			PyErr_SetNone(PyExc_KeyError);
			return NULL;
		}
	}
#undef automaton
}

typedef struct AutomatonQueueItem {
	LISTITEM_data;
	TrieNode*	node;
} AutomatonQueueItem;

#define automaton_make_automaton_doc \
	"convert trie to Aho-Corasick automaton"

static PyObject*
automaton_make_automaton(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	if (automaton->kind != TRIE)
		Py_RETURN_FALSE;
	
	AutomatonQueueItem* item;
	List queue;
	int i;

	list_init(&queue);

	// 1. setup nodes at 1-st level
	ASSERT(automaton->root);

	for (i=0; i < 256; i++) {
		TrieNode* child = trienode_get_next(automaton->root, i);
		if (child) {
			// fail edges go to root
			child->fail = automaton->root;

			item = (AutomatonQueueItem*)list_item_new(sizeof(AutomatonQueueItem));
			if (item) {
				item->node = child;
				list_append(&queue, (ListItem*)item);
			}
			else
				goto no_mem;
		}
		else
			// loop on root - implicit (see automaton_next)
			;
	}

	// 2. make links
	TrieNode* node;
	TrieNode* child;
	TrieNode* state;
	while (true) {
		AutomatonQueueItem* item = (AutomatonQueueItem*)list_pop_first(&queue);
		if (item == NULL)
			break;
		else
			node = item->node;

		const size_t n = node->n;
		for (i=0; i < n; i++) {
			child = node->next[i];
			ASSERT(child);

			item = (AutomatonQueueItem*)list_item_new(sizeof(AutomatonQueueItem));
			item->node = child;
			if (item)
				list_append(&queue, (ListItem*)item);
			else
				goto no_mem;

			state = node->fail;
			ASSERT(state);
			ASSERT(child);
			while (state != automaton->root and\
				   not trienode_get_next(state, child->byte)) {

				state = state->fail;
				ASSERT(state);
			}

			child->fail = trienode_get_next(state, child->byte);
			if (child->fail == NULL)
				child->fail = automaton->root;
			
			ASSERT(child->fail);
		}
	}

	automaton->kind = AHOCORASICK;
	list_delete(&queue);
	Py_RETURN_NONE;
#undef automaton

no_mem:
	list_delete(&queue);
	PyErr_NoMemory();
	return NULL;
}


#define automaton_search_all_doc \
	"search_all(string, callback, [start, [end]])"

static PyObject*
automaton_search_all(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	if (automaton->kind != AHOCORASICK)
		Py_RETURN_NONE;

	ssize_t wordlen;
	ssize_t start;
	ssize_t end;
	char* word;
	bool unicode;
	PyObject* py_word;
	PyObject* callback;
	PyObject* callback_ret;

	// arg 1
	py_word = pymod_get_string(args, 0, &word, &wordlen, &unicode);
	if (py_word == NULL)
		return NULL;

	// arg 2
	callback = PyTuple_GetItem(args, 1);
	if (callback == NULL)
		return NULL;
	else
	if (not PyCallable_Check(callback)) {
		PyErr_SetString(PyExc_TypeError, "second argument isn't callable");
		return NULL;
	}

	// parse start/end
	if (pymod_parse_start_end(args, 2, 3, 0, wordlen, &start, &end))
		return NULL;

	ssize_t i;
	TrieNode* state;
	TrieNode* tmp;

	state = automaton->root;
	for (i=start; i < end; i++) {
#define NEXT(byte) ahocorasick_next(state, automaton->root, byte)
		if (unicode) {
#ifndef Py_UNICODE_WIDE
			const uint16_t w = ((uint16_t*)word)[i];
			state = NEXT(w & 0xff);
			if (w > 0x00ff)
				state = NEXT((w >> 8) & 0xff);
#else
			const uint32_t w = ((uint32_t*)word)[i];
			state = NEXT(w & 0xff);
			if (w > 0x000000ff) {
				state = NEXT((w >> 8) & 0xff);
				if (w > 0x0000ffff) {
					state = NEXT((w >> 16) & 0xff);
					if (w > 0x00ffffff) {
						state = NEXT((w >> 24) & 0xff);
					}
				}
			}
#endif
			tmp = state;
		}
		else
			state = tmp = ahocorasick_next(state, automaton->root, word[i]);
#undef NEXT

		// return output
		while (tmp and tmp->output) {
			callback_ret = PyObject_CallFunction(callback, "iO", i, tmp->output);
			if (callback_ret == NULL)
				return NULL;
			else
				Py_DECREF(callback_ret);

			tmp = tmp->fail;
		}
	}
#undef automaton

	Py_RETURN_NONE;
}


#define automaton_keys_doc \
	"iterator for keys"

static PyObject*
automaton_keys(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	AutomatonItemsIter* iter = (AutomatonItemsIter*)automaton_items_iter_new(automaton);
	if (iter) {
		iter->type = ITER_KEYS;
		return (PyObject*)iter;
	}
	else
		return NULL;
#undef automaton
}


#define automaton_values_doc \
	"iterator for values"

static PyObject*
automaton_values(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	AutomatonItemsIter* iter = (AutomatonItemsIter*)automaton_items_iter_new(automaton);
	if (iter) {
		iter->type = ITER_VALUES;
		return (PyObject*)iter;
	}
	else
		return NULL;
#undef automaton
}


#define automaton_items_doc \
	"iterator for items"

static PyObject*
automaton_items(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)
	AutomatonItemsIter* iter = (AutomatonItemsIter*)automaton_items_iter_new(automaton);
	if (iter) {
		iter->type = ITER_ITEMS;
		return (PyObject*)iter;
	}
	else
		return NULL;
#undef automaton
}


#define automaton_iter_doc \
	"iter(string|buffer, [start, [end]])"

static PyObject*
automaton_iter(PyObject* self, PyObject* args) {
#define automaton ((Automaton*)self)

	if (automaton->kind != AHOCORASICK) {
		PyErr_SetString(PyExc_AttributeError, "not an automaton yet; add some words and call make_automaton");
		return NULL;
	}

	PyObject* object;
	bool is_unicode;
	ssize_t start;
	ssize_t end;

	object = PyTuple_GetItem(args, 0);
	if (object) {
		if (PyUnicode_Check(object)) {
			is_unicode = true;
			start	= 0;
			end		= PyUnicode_GET_SIZE(object);
		}
		else
		if (PyBytes_Check(object)) {
			is_unicode = false;
			start 	= 0;
			end		= PyBytes_GET_SIZE(object);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "string or bytes object required");
			return NULL;
		}
	}
	else
		return NULL;

	if (pymod_parse_start_end(args, 1, 2, start, end, &start, &end))
		return NULL;

	return automaton_search_iter_new(
		automaton,
		object,
		start,
		end,
		is_unicode
	);
#undef automaton
}


#define method(name, kind) {#name, automaton_##name, kind, automaton_##name##_doc}
static
PyMethodDef automaton_methods[] = {
	method(add_word,		METH_VARARGS),
	method(clear,			METH_NOARGS),
	method(match,			METH_VARARGS),
	method(match_prefix,	METH_VARARGS),
	method(get,				METH_VARARGS),
	method(make_automaton,	METH_NOARGS),
	method(search_all,		METH_VARARGS),
	method(keys,			METH_NOARGS),
	method(values,			METH_NOARGS),
	method(items,			METH_NOARGS),
	method(iter,			METH_VARARGS),

	{NULL, NULL, 0, NULL}
};
#undef method


static
PySequenceMethods automaton_as_sequence;


static
PyMemberDef automaton_members[] = {
	{
		"kind",
		T_INT,
		offsetof(Automaton, kind),
		READONLY,
		"current kind of automaton"
	},

	{NULL}
};

static PyTypeObject automaton_type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"Automaton",								/* tp_name */
	sizeof(Automaton),							/* tp_size */
	0,											/* tp_itemsize? */
	(destructor)automaton_del,          	  	/* tp_dealloc */
	0,                                      	/* tp_print */
	0,                                         	/* tp_getattr */
	0,                                          /* tp_setattr */
	0,                                          /* tp_reserved */
	0,											/* tp_repr */
	0,                                          /* tp_as_number */
	0,                                          /* tp_as_sequence */
	0,                                          /* tp_as_mapping */
	0,                                          /* tp_hash */
	0,                                          /* tp_call */
	0,                                          /* tp_str */
	PyObject_GenericGetAttr,                    /* tp_getattro */
	0,                                          /* tp_setattro */
	0,                                          /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                         /* tp_flags */
	0,                                          /* tp_doc */
	0,                                          /* tp_traverse */
	0,                                          /* tp_clear */
	0,                                          /* tp_richcompare */
	0,                                          /* tp_weaklistoffset */
	0,                                          /* tp_iter */
	0,                                          /* tp_iternext */
	automaton_methods,							/* tp_methods */
	automaton_members,			                /* tp_members */
	0,                                          /* tp_getset */
	0,                                          /* tp_base */
	0,                                          /* tp_dict */
	0,                                          /* tp_descr_get */
	0,                                          /* tp_descr_set */
	0,                                          /* tp_dictoffset */
	0,                                          /* tp_init */
	0,                                          /* tp_alloc */
	0,                                          /* tp_new */
};
