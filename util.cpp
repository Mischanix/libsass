#include "util.hpp"
#include "context.hpp"
#include "to_string.hpp"

namespace Sass {
  namespace Util {


    using std::string;


    string normalize_underscores(const string& str) {
      string normalized = str;
      for(size_t i = 0, L = normalized.length(); i < L; ++i) {
        if(normalized[i] == '_') {
          normalized[i] = '-';
        }
      }
      return normalized;
    }


    bool isPrintable(Ruleset* r) {
      if (r == NULL) {
        return false;
      }

      Block* b = r->block();
      
      bool hasSelectors = static_cast<Selector_List*>(r->selector())->length() > 0;
      
      if (!hasSelectors) {
      	return false;
      }

      bool hasDeclarations = false;
      bool hasPrintableChildBlocks = false;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement* stm = (*b)[i];

        if (dynamic_cast<Has_Block*>(stm)) {
          Block* pChildBlock = ((Has_Block*)stm)->block();
          if (isPrintable(pChildBlock)) {
            hasPrintableChildBlocks = true;
          }
        } else {
        	hasDeclarations = true;
        }
        
        if (hasDeclarations || hasPrintableChildBlocks) {
        	return true;
        }
      }
      
      return false;
    }


    bool isPrintable(Media_Block* m) {
      if (m == NULL) {
        return false;
      }
  
      Block* b = m->block();

      bool hasSelectors = m->selector() && static_cast<Selector_List*>(m->selector())->length() > 0;
      
      bool hasDeclarations = false;
      bool hasPrintableChildBlocks = false;
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement* stm = (*b)[i];
        if (!stm->is_hoistable() && m->selector() != NULL && !hasSelectors) {
        	// If a statement isn't hoistable, the selectors apply to it. If there are no selectors (a selector list of length 0),
          // then those statements aren't considered printable. That means there was a placeholder that was removed. If the selector
          // is NULL, then that means there was never a wrapping selector and it is printable (think of a top level media block with
          // a declaration in it).
        }
        else if (typeid(*stm) == typeid(Declaration) || typeid(*stm) == typeid(At_Rule)) {
          hasDeclarations = true;
        }
        else if (dynamic_cast<Has_Block*>(stm)) {
        	Block* pChildBlock = ((Has_Block*)stm)->block();
          if (isPrintable(pChildBlock)) {
            hasPrintableChildBlocks = true;
          }
        }

        if (hasDeclarations || hasPrintableChildBlocks) {
          return true;
        }
      }
      
      return false;
    }


    bool isPrintable(Block* b) {
      if (b == NULL) {
        return false;
      }

      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement* stm = (*b)[i];
        if (typeid(*stm) == typeid(Declaration) || typeid(*stm) == typeid(At_Rule)) {
          return true;
        }
        else if (typeid(*stm) == typeid(Ruleset)) {
          Ruleset* r = (Ruleset*) stm;
          if (isPrintable(r)) {
            return true;
          }
        }
        else if (typeid(*stm) == typeid(Media_Block)) {
          Media_Block* m = (Media_Block*) stm;
          if (isPrintable(m)) {
            return true;
          }
        }
        else if (dynamic_cast<Has_Block*>(stm) && isPrintable(((Has_Block*)stm)->block())) {
          return true;
        }
      }
      
      return false;
    }

  
    typedef deque<List*> MediaQueryStack;
    typedef deque<Media_Block*> MediaBlocks;

    
    static List* createCombinedMediaQueries(MediaQueryStack& queryStack, Context& ctx) {
      // TODO: should we get source and position from the front of back of the stack?
      List* pLastList = queryStack.back();

      List* pCombined = new (ctx.mem) List(pLastList->path(), pLastList->position());
      Media_Query* pCombinedQuery = new (ctx.mem) Media_Query(pLastList->path(), pLastList->position());
      (*pCombined) << pCombinedQuery;
      
      Media_Query* pCopyFromFirst = dynamic_cast<Media_Query*>((*queryStack.front())[0]);
      pCombinedQuery->media_type(pCopyFromFirst->media_type());
      pCombinedQuery->is_negated(pCopyFromFirst->is_negated());
      pCombinedQuery->is_restricted(pCopyFromFirst->is_restricted());
      
      /*
      Media_Query* qq = new (ctx.mem) Media_Query(q->path(),
                                                  q->position(),
                                                  t,
                                                  q->length(),
                                                  q->is_negated(),
                                                  q->is_restricted());*/
      
      //List* pCombined = queryStack.front();
      //queryStack.pop_front();
      
      //Media_Query* pCombinedQuery = dynamic_cast<Media_Query*>((*pCombined)[0]);
      
      // TODO: are there other parts of the media query that need to be merged?
      
      for (MediaQueryStack::iterator iter = queryStack.begin(), iterEnd = queryStack.end(); iter != iterEnd; iter++) {
        List* pNextList = *iter;
        
        for (size_t i = 0, L = pNextList->length(); i < L; ++i) {
          Media_Query* pMediaQuery = dynamic_cast<Media_Query*>((*pNextList)[i]);
          
          for (size_t i = 0, L = pMediaQuery->length(); i < L; ++i) {
            *pCombinedQuery << (*pMediaQuery)[i];
          }
        }
      }

      return pCombined;
    }

    
    static Block* bubbleMediaQueries(Block* pBlock, MediaQueryStack& queryStack, MediaBlocks& bubbledBlocks, Context& ctx) {
      if (pBlock == NULL) {
        return pBlock;
      }

      Block* pNewBlock = new (ctx.mem) Block(pBlock->path(), pBlock->position(), pBlock->length(), pBlock->is_root());

      for (size_t i = 0, L = pBlock->length(); i < L; ++i) {
        Statement* pStm = (*pBlock)[i];

        if (typeid(*pStm) == typeid(Media_Block)) {

          Media_Block* pChildMediaBlock = dynamic_cast<Media_Block*>(pStm);

          queryStack.push_back(pChildMediaBlock->media_queries());
          
          pChildMediaBlock->media_queries(createCombinedMediaQueries(queryStack, ctx));
          
          bubbledBlocks.push_back(pChildMediaBlock);
          
          // Purposefully don't push the block onto the new block. It's going to be returned in bubbledBlocks instead
          
          Block* pNewChildMediaBlockBlock = bubbleMediaQueries(pChildMediaBlock->block(), queryStack, bubbledBlocks, ctx);
          pChildMediaBlock->block(pNewChildMediaBlockBlock);
          
          queryStack.pop_back();

        } else if (dynamic_cast<Has_Block*>(pStm)) {

          Has_Block* pStmHasBlock = (Has_Block*) pStm;
          Block* pChildBlock = pStmHasBlock->block();
          
          
          Block* pNewStmBlock = NULL;
					if (typeid(*pStm) == typeid(Ruleset)) {
	          pNewStmBlock = bubbleMediaQueries(pChildBlock, queryStack, bubbledBlocks, ctx);
          } else {
          	pNewStmBlock = bubbleMediaQueriesTopLevel(pChildBlock, ctx);
          }

          pStmHasBlock->block(pNewStmBlock);

          *pNewBlock << pStm;

        } else {
        
          *pNewBlock << pStm;
        
        }
      }
      
      return pNewBlock;
    
    }


    Block* bubbleMediaQueriesTopLevel(Block* pBlock, Context& ctx) {
      if (pBlock == NULL) {
        return pBlock;
      }

      Block* pNewBlock = new (ctx.mem) Block(pBlock->path(), pBlock->position(), pBlock->length(), pBlock->is_root());
      
      for (size_t i = 0, L = pBlock->length(); i < L; ++i) {
        Statement* pStm = (*pBlock)[i];

        if (dynamic_cast<Has_Block*>(pStm)) {

          Has_Block* pStmHasBlock = (Has_Block*) pStm;
          Block* pChildBlock = ((Has_Block*) pStm)->block();

          MediaQueryStack queryStack;
          MediaBlocks bubbledBlocks;
          
          if (typeid(*pStm) == typeid(Media_Block)) {
            Media_Block* pChildMediaBlock = dynamic_cast<Media_Block*>(pStm);
            queryStack.push_back(pChildMediaBlock->media_queries());
          }
          
          Block* pNewStmBlock = bubbleMediaQueries(pChildBlock, queryStack, bubbledBlocks, ctx);
          pStmHasBlock->block(pNewStmBlock);
          
          // Add our new Media_Block*s after this statement

          *pNewBlock << pStm;

          for (MediaBlocks::iterator iter = bubbledBlocks.begin(), iterEnd = bubbledBlocks.end(); iter != iterEnd; iter++) {
            *pNewBlock << *iter;
          }
          
          if (typeid(*pStm) == typeid(Media_Block)) {
            queryStack.pop_back();
          }

        } else {

          *pNewBlock << pStm;

        }
      }
      
      return pNewBlock;
    }

  }

}
