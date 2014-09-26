#include "util.hpp"
#include "context.hpp"

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
    
    // A Block is considered printable if a Declaration or At_Rule is found anywhere under its recursive structure
    bool containsAnyPrintableStatements(Block* b) {
      
      for (size_t i = 0, L = b->length(); i < L; ++i) {
        Statement* stm = (*b)[i];
        if (typeid(*stm) == typeid(Declaration) || typeid(*stm) == typeid(At_Rule)) {
          return true;
        }
        else if (dynamic_cast<Has_Block*>(stm) && containsAnyPrintableStatements(((Has_Block*)stm)->block())) {
          return true;
        }
      }
      
      return false;
    }
    
  }
  
  typedef deque<List*> MediaQueryStack;
  typedef deque<Media_Block*> MediaBlocks;
  
  static List* createCombinedMediaQueries(MediaQueryStack& queryStack) {
  	return queryStack.back();
  }
  
  static void bubbleMediaQueries(Block*& pBlock, MediaQueryStack& queryStack, MediaBlocks& bubbledBlocks, Context& ctx) {

		Block* pNewBlock = new (ctx.mem) Block(pBlock->path(), pBlock->position(), pBlock->length(), pBlock->is_root());

    for (size_t i = 0, L = pBlock->length(); i < L; ++i) {
      Statement* pStm = (*pBlock)[i];

      if (typeid(*pStm) == typeid(Media_Block)) {

        Media_Block* pChildMediaBlock = dynamic_cast<Media_Block*>(pStm);

        queryStack.push_back(pChildMediaBlock->media_queries());
        
				pChildMediaBlock->media_queries(createCombinedMediaQueries(queryStack));
        
        bubbledBlocks.push_back(pChildMediaBlock);
        
        // Purposefully don't push the block onto the new block. It's going to be returned in bubbledBlocks instead

      } else if (dynamic_cast<Has_Block*>(pStm)) {

        Block* pChildBlock = ((Has_Block*) pStm)->block();
        bubbleMediaQueries(pChildBlock, queryStack, bubbledBlocks, ctx);
        *pNewBlock << pChildBlock;

      }
    }
    
    pBlock = pNewBlock;
  
  }

	void bubbleMediaQueries(Block*& pBlock, Context& ctx) {
  
  	Block* pNewBlock = new (ctx.mem) Block(pBlock->path(), pBlock->position(), pBlock->length(), pBlock->is_root());
    
    for (size_t i = 0, L = pBlock->length(); i < L; ++i) {
      Statement* pStm = (*pBlock)[i];

      if (dynamic_cast<Has_Block*>(pStm)) {

        Block* pChildBlock = ((Has_Block*) pStm)->block();

        MediaQueryStack queryStack;
        MediaBlocks bubbledBlocks;
        
        if (typeid(*pStm) == typeid(Media_Block)) {
        	Media_Block* pChildMediaBlock = dynamic_cast<Media_Block*>(pStm);
        	queryStack.push_back(pChildMediaBlock->media_queries());
        }
        
        bubbleMediaQueries(pChildBlock, queryStack, bubbledBlocks, ctx);
        
        // Add our new Media_Block*s after this statement

        *pNewBlock << pChildBlock;

        for (MediaBlocks::iterator iter = bubbledBlocks.begin(), iterEnd = bubbledBlocks.end(); iter != iterEnd; iter++) {
        	*pNewBlock << *iter;
        }

      } else {

      	*pNewBlock << pStm;

      }
    }
    
    pBlock = pNewBlock;
  }

}
