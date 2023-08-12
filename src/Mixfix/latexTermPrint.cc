/*

    This file is part of the Maude 3 interpreter.

    Copyright 2023 SRI International, Menlo Park, CA 94025, USA.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.

*/

//
//	Term* -> ostream& LaTeX pretty printer.
//

void
MixfixModule::latexPrettyPrint(ostream& s, Term* term)
{
  globalIndent = 0;
  MixfixModule* module = safeCastNonNull<MixfixModule*>(term->symbol()->getModule());
  module->latexPrettyPrint(s, term, UNBOUNDED, UNBOUNDED, 0, UNBOUNDED, 0, false);
}

void
MixfixModule::latexPrintStrategyTerm(ostream& s, RewriteStrategy* rs, Term* term) const
{
  s << Token::latexIdentifier(rs->id());
  if (rs->arity() > 0 || ruleLabels.find(rs->id()) != ruleLabels.end())
    {
      s << "\\maudeLeftParen";
      const char* sep = "";
      for (ArgumentIterator it(*term); it.valid(); it.next())
	{
	  s << sep;
	  latexPrettyPrint(s, it.argument());
	  sep = "\\maudeComma";
	}
      s << "\\maudeRightParen";
    }
}

const char*
MixfixModule::latexComputeColor(SymbolType st)
{
  if (interpreter.getPrintFlag(Interpreter::PRINT_COLOR))
    {
      if (st.hasFlag(SymbolType::ASSOC))
	{
	  if (st.hasFlag(SymbolType::COMM))
	    return st.hasFlag(SymbolType::LEFT_ID | SymbolType::RIGHT_ID) ? latexMagenta : latexRed;
	  else
	    return st.hasFlag(SymbolType::LEFT_ID | SymbolType::RIGHT_ID) ? latexCyan :latexGreen;
	}
      if (st.hasFlag(SymbolType::COMM))
	return latexBlue;
      if (st.hasFlag(SymbolType::LEFT_ID | SymbolType::RIGHT_ID | SymbolType::IDEM))
	return latexYellow;
    }
  return 0;
}

void
MixfixModule::latexSuffix(ostream& s, Term* term, bool needDisambig, const char* /* color */)
{
  if (needDisambig)
    s << "\\maudeRightParen\\maudeDisambigDot" << latexType(disambiguatorSort(term));
}

bool
MixfixModule::latexHandleIter(ostream& s, Term* term, const SymbolInfo& si, bool rangeKnown, const char* color)
{
  //
  //	Check if term is headed by a iter symbol and if so handle
  //	any special printing requirements.
  //	Returns true if handled, false if default handling required.
  //
  if (!(si.symbolType.hasFlag(SymbolType::ITER)))
    return false;
  //
  //	Check if the top symbol is also a succ symbol and we have number printing turned on.
  //
  if (si.symbolType.getBasicType() == SymbolType::SUCC_SYMBOL &&
      interpreter.getPrintFlag(Interpreter::PRINT_NUMBER))
    {
      //
      //	If term  corresponds to a number we want to print it as decimal number.
      //
      SuccSymbol* succSymbol = safeCastNonNull<SuccSymbol*>(term->symbol());
      if (succSymbol->isNat(term))
	{
	  const mpz_class& nat = succSymbol->getNat(term);
	  bool needDisambig = !rangeKnown && (kindsWithSucc.size() > 1 || overloadedIntegers.count(nat));
	  latexPrefix(s, needDisambig, color);
	  s << "\\maudeNumber{" << nat << "}";
	  latexSuffix(s, term, needDisambig, color);
	  return true;
	}
    }
  //
  //	Not going to print term as a number, but might want to use special f^n(arg) representation.
  //
  S_Term* st = safeCastNonNull<S_Term*>(term);
  const mpz_class& number = st->getNumber();
  if (number == 1)
    return false;  // do default thing
  
  bool needToDisambiguate;
  bool argumentRangeKnown;
  decideIteratedAmbiguity(rangeKnown, term->symbol(), number, needToDisambiguate, argumentRangeKnown);
  if (needToDisambiguate)
    s << "\\maudeLeftParen";

  string prefixName = Token::latexIdentifier(term->symbol()->id()) +
    "^{\\maudeNumber{" + number.get_str() + "}}";
  if (color != 0)
    s << color << prefixName << latexResetColor;
  else
    latexPrintPrefixName(s, prefixName.c_str(), si);
  s << "\\maudeLeftParen";
  latexPrettyPrint(s, st->getArgument(), PREFIX_GATHER, UNBOUNDED, 0, UNBOUNDED, 0, argumentRangeKnown);
  s << "\\maudeRightParen";
  suffix(s, term, needToDisambiguate, color);
  return true;
}

bool
MixfixModule::latexHandleMinus(ostream& s, Term* term, bool rangeKnown, const char* color) const
{
  if (interpreter.getPrintFlag(Interpreter::PRINT_NUMBER))
    {
      const MinusSymbol* minusSymbol = safeCast(MinusSymbol*, term->symbol());
      if (minusSymbol->isNeg(term))
	{
	  mpz_class neg;
	  (void) minusSymbol->getNeg(term, neg);
	  bool needDisambig = !rangeKnown && (kindsWithMinus.size() > 1 || overloadedIntegers.count(neg));
	  latexPrefix(s, needDisambig, color);
	  s << "\\maudeNumber{" << neg << "}";
	  latexSuffix(s, term, needDisambig, color);
	  return true;
	}
    }
  return false;
}

bool
MixfixModule::latexHandleDivision(ostream& s, Term* term, bool rangeKnown, const char* color) const
{
  if (interpreter.getPrintFlag(Interpreter::PRINT_RAT))
    {
      const DivisionSymbol* divisionSymbol = safeCast(DivisionSymbol*, term->symbol());
      if (divisionSymbol->isRat(term))
	{
	  pair<mpz_class, mpz_class> rat;
	  rat.second = divisionSymbol->getRat(term, rat.first);
	  bool needDisambig = !rangeKnown && (kindsWithDivision.size() > 1 || overloadedRationals.count(rat));
	  latexPrefix(s, needDisambig, color);
	  s << "\\maudeNumber{" << rat.first << "}/\\maudeNumber{"  << rat.second << "}";
	  latexSuffix(s, term, needDisambig, color);
	  return true;
	}
    }
  return false;
}

void
MixfixModule::latexHandleFloat(ostream& s, Term* term, bool rangeKnown, const char* color) const
{
  double mfValue = safeCastNonNull<FloatTerm*>(term)->getValue();
  bool needDisambig = !rangeKnown && (floatSymbols.size() > 1 || overloadedFloats.count(mfValue));
  latexPrefix(s, needDisambig, color);
  s << "\\maudeNumber{" << doubleToString(mfValue) << "}";
  latexSuffix(s, term, needDisambig, color);
}

void
MixfixModule::latexHandleString(ostream& s, Term* term, bool rangeKnown, const char* color) const
{
  string strValue;
  Token::ropeToString(safeCastNonNull<StringTerm*>(term)->getValue(), strValue);
  bool needDisambig = !rangeKnown && (stringSymbols.size() > 1 || overloadedStrings.count(strValue));
  latexPrefix(s, needDisambig, color);
  s << "\\maudeString{" << Token::latexName(strValue) << "}";
  latexSuffix(s, term, needDisambig, color);
}

void
MixfixModule::latexHandleQuotedIdentifier(ostream& s, Term* term, bool rangeKnown, const char* color) const
{
  int qidCode = safeCastNonNull<QuotedIdentifierTerm*>(term)->getIdIndex();
  bool needDisambig = !rangeKnown && (quotedIdentifierSymbols.size() > 1 || overloadedQuotedIdentifiers.count(qidCode));
  latexPrefix(s, needDisambig, color);
  s << "\\maudeQid{" << "\\maudeSingleQuote " << Token::latexName(qidCode) << "}";
  latexSuffix(s, term, needDisambig, color);
}

void
MixfixModule::latexHandleVariable(ostream& s, Term* term, bool rangeKnown, const char* color) const
{
  VariableTerm* v = safeCastNonNull<VariableTerm*>(term);
  Sort* sort = safeCastNonNull<VariableSymbol*>(term->symbol())->getSort();
  pair<int, int> p(v->id(), sort->id());
  bool needDisambig = !rangeKnown && overloadedVariables.count(p);  // kinds not handled
  latexPrefix(s, needDisambig, color);
  s << Token::latexIdentifier(v->id());
  
  if (interpreter.getPrintFlag(Interpreter::PRINT_WITH_ALIASES))
    {
      AliasMap::const_iterator i = variableAliases.find(v->id());
      if (i != variableAliases.end() && (*i).second == sort)
	{
	  latexSuffix(s, term, needDisambig, color);
	  return;
	}
    }
  s << "\\maudeVariableColon" << latexType(sort);
  latexSuffix(s, term, needDisambig, color);
}

void
MixfixModule::latexHandleSMT_Number(ostream& s, Term* term, bool rangeKnown, const char* color)
{
  //
  //	Get value.
  //
  SMT_NumberTerm* n = safeCastNonNull<SMT_NumberTerm*>(term);
  const mpq_class& value = n->getValue();
  //
  //	Look up the index of our sort.
  //
  Symbol* symbol = term->symbol();
  Sort* sort = symbol->getRangeSort();
  //
  //	Figure out what SMT sort we correspond to.
  //
  SMT_Info::SMT_Type t = getSMT_Info().getType(sort);
  Assert(t != SMT_Info::NOT_SMT, "bad SMT sort " << sort);
  if (t == SMT_Info::INTEGER)
    {
      const mpz_class& integer = value.get_num();
      bool needDisambig = !rangeKnown && (kindsWithSucc.size() > 1 || overloadedIntegers.count(integer));
      latexPrefix(s, needDisambig, color);
      s << "\\maudeNumber{" << integer << "}";
      latexSuffix(s, term, needDisambig, color);
    }
  else
    {
      Assert(t == SMT_Info::REAL, "SMT number sort expected");
      pair<mpz_class, mpz_class> rat(value.get_num(), value.get_den());
      bool needDisambig = !rangeKnown && (kindsWithDivision.size() > 1 || overloadedRationals.count(rat));
      latexPrefix(s, needDisambig, color);
      s << "\\maudeNumber{" << rat.first << "}/\\maudeNumber{" << rat.second << "}";
      latexSuffix(s, term, needDisambig, color);
    }
}

void
MixfixModule::latexPrettyPrint(ostream& s,
			       Term* term,
			       int requiredPrec,
			       int leftCapture,
			       const ConnectedComponent* leftCaptureComponent,
			       int rightCapture,
			       const ConnectedComponent* rightCaptureComponent,
			       bool rangeKnown)
{
  if (UserLevelRewritingContext::interrupted())
    return;

  Symbol* symbol = term->symbol();
  const SymbolInfo& si = symbolInfo[symbol->getIndexWithinModule()];
  const char* color = computeColor(si.symbolType);
  //
  //	Check for special i/o representation.
  //
  if (latexHandleIter(s, term, si, rangeKnown, color))
    return;
  int basicType = si.symbolType.getBasicType();
  switch (basicType)
    {
    case SymbolType::MINUS_SYMBOL:
      {
	if (latexHandleMinus(s, term, rangeKnown, color))
	  return;
	break;
      }
    case SymbolType::DIVISION_SYMBOL:
      {
	if (latexHandleDivision(s, term, rangeKnown, color))
	  return;
	break;
      }
    case SymbolType::FLOAT:
      {
	latexHandleFloat(s, term, rangeKnown, color);
	return;
      }
    case SymbolType::STRING:
      {
	latexHandleString(s, term, rangeKnown, color);
	return;
      }
    case SymbolType::QUOTED_IDENTIFIER:
      {
	latexHandleQuotedIdentifier(s, term, rangeKnown, color);
	return;
      }
    case SymbolType::VARIABLE:
      {
	latexHandleVariable(s, term, rangeKnown, color);
	return;
      }
    case SymbolType::SMT_NUMBER_SYMBOL:
      {
	latexHandleSMT_Number(s, term, rangeKnown, color);
	return;
      }
    default:
      break;
    }
  //
  //	Default case where no special i/o representation applies.
  //
  int iflags = si.iflags;
  bool needDisambig = !rangeKnown && ambiguous(iflags);
  bool argRangeKnown = false;
  int nrArgs = symbol->arity();
  if (nrArgs == 0)
    {
      if (interpreter.getPrintFlag(Interpreter::PRINT_DISAMBIG_CONST))
	needDisambig = true;
    }
  else
    argRangeKnown = rangeOfArgumentsKnown(iflags, rangeKnown, needDisambig);

 if (needDisambig)
    s << "\\maudeLeftParen";

 if (nrArgs == 0 && Token::auxProperty(symbol->id()) == Token::AUX_STRUCTURED_SORT)
   s << latexStructuredConstant(symbol->id());
 else if ((interpreter.getPrintFlag(Interpreter::PRINT_MIXFIX) && !si.mixfixSyntax.empty()) || basicType == SymbolType::SORT_TEST)
   {
     //
     //	Mixfix case.
     //
     bool printWithParens = interpreter.getPrintFlag(Interpreter::PRINT_WITH_PARENS);
     bool needParen = !needDisambig &&
       (printWithParens || requiredPrec < si.prec ||
	((iflags & LEFT_BARE) && leftCapture <= si.gather[0] &&
	  leftCaptureComponent == symbol->domainComponent(0)) ||
	((iflags & RIGHT_BARE) && rightCapture <= si.gather[nrArgs - 1] &&
	 rightCaptureComponent == symbol->domainComponent(nrArgs - 1)));
     bool needAssocParen = si.symbolType.hasFlag(SymbolType::ASSOC) &&
       (printWithParens || si.gather[1] < si.prec ||
	((iflags & LEFT_BARE) && (iflags & RIGHT_BARE) &&
	 si.prec <= si.gather[0]));
     if (needParen)
       s << "\\maudeLeftParen";
     int nrTails = 1;
     int pos = 0;
     ArgumentIterator a(*term);
     int moreArgs = a.valid();
     for (int arg = 0; moreArgs; arg++)
       {
	 Term* t = a.argument();
	 a.next();
	 moreArgs = a.valid();
	 pos = latexPrintTokens(s, si, pos, color);
	 if (arg == nrArgs - 1 && moreArgs)
	   {
	     ++nrTails;
	     arg = 0;
	     if (needAssocParen)
	       s << "\\maudeLeftParen";
	     pos = latexPrintTokens(s, si, 0, color);
	   }
	 int lc = UNBOUNDED;
	 const ConnectedComponent* lcc = 0;
	 int rc = UNBOUNDED;
	 const ConnectedComponent* rcc = 0;
	 if (arg == 0 && (iflags & LEFT_BARE))
	   {
	     rc = si.prec;
	     rcc = symbol->domainComponent(0);
	     if (!needParen && !needDisambig)
	       {
		 lc = leftCapture;
		 lcc = leftCaptureComponent;
	       }
	   }
	 else if (!moreArgs && (iflags & RIGHT_BARE))
	   {
	     lc = si.prec;
	     lcc = symbol->domainComponent(nrArgs - 1);
	     if (!needParen && !needDisambig)
	       {
		 rc = rightCapture;
		 rcc = rightCaptureComponent;
	       }
	   }
	 latexPrettyPrint(s, t, si.gather[arg], lc, lcc, rc, rcc, argRangeKnown);
	 if (UserLevelRewritingContext::interrupted())
	   return;
       }
     latexPrintTails(s, si, pos, nrTails, needAssocParen, true, color);
     if (needParen)
       s << "\\maudeRightParen";
   }
 else
   {
     //
     //	Prefix case.
     //
     string prefixName = Token::latexIdentifier(symbol->id());
     if (color != 0)
       s << color << prefixName << latexResetColor;
     else
       latexPrintPrefixName(s, prefixName.c_str(), si);
     ArgumentIterator a(*term);
     if (a.valid())
       {
	 int nrTails = 1;
	 s << "\\maudeLeftParen";
	 for (int arg = 0;; arg++)
	   {
	     Term* t = a.argument();
	     a.next();
	     int moreArgs = a.valid();
	     if (arg >= nrArgs - 1 &&
		 !(interpreter.getPrintFlag(Interpreter::PRINT_FLAT)) &&
		 moreArgs)
	       {
		 ++nrTails;
		 if (color != 0)
		   s << color << prefixName << latexResetColor;
		 else
		   latexPrintPrefixName(s, prefixName.c_str(), si);
		 s << "\\maudeLeftParen";
	       }
	     latexPrettyPrint(s, t, PREFIX_GATHER, UNBOUNDED, 0, UNBOUNDED, 0, argRangeKnown);
	     if (!moreArgs)
	       break;
	     s << "\\maudeComma";
	   }
	 while (nrTails-- > 0)
	   {
	     if (UserLevelRewritingContext::interrupted())
	       return;
	     s << "\\maudeRightParen";
	   }
       }
   }
 latexSuffix(s, term, needDisambig, color);
}