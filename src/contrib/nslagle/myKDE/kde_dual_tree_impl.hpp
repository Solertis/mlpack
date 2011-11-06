
#ifndef KDE_DUAL_TREE_IMPL_HPP
#define KDE_DUAL_TREE_IMPL_HPP

#ifndef KDE_DUAL_TREE_HPP
#ifndef USE_KDE_DUAL_TREE_IMPL_HPP
#error "Do not include this header directly."
#endif
#endif

#define MADEIT std::cout<<"made it to "<<__LINE__<<" in "<<__FILE__<<std::endl

using namespace mlpack;
using namespace mlpack::kde;

namespace mlpack
{
namespace kde
{

template<typename TKernel, typename TTree>
KdeDualTree<TKernel, TTree>::KdeDualTree (arma::mat& reference,
                                   arma::mat& query)
{
  referenceRoot = new TTree (reference, referenceShuffledIndices),
  queryRoot = new TTree (query, queryShuffledIndices);
  referenceData = reference;
  queryData = query;
  levelsInTree = queryRoot->levelsBelow();
  queryTreeSize = queryRoot->treeSize();
  SetDefaults();
}

template<typename TKernel, typename TTree>
KdeDualTree<TKernel, TTree>::KdeDualTree (arma::mat& reference)
{
  referenceData = reference;
  queryData = reference;

  referenceRoot = new TTree (reference, referenceShuffledIndices);
  queryRoot = referenceRoot;
  queryShuffledIndices = referenceShuffledIndices;
  levelsInTree = queryRoot->levelsBelow();
  queryTreeSize = queryRoot->treeSize();
  SetDefaults();
}

template<typename TKernel, typename TTree>
void KdeDualTree<TKernel, TTree>::SetDefaults()
{
  SetBandwidthBounds(0.01, 100.0);
  bandwidthCount = 10;
  delta = epsilon = 0.05;
  kernel = TKernel(1.0);
  nextAvailableNodeIndex = 0;
}

template<typename TKernel, typename TTree>
std::vector<double> KdeDualTree<TKernel, TTree>::Calculate()
{
  /* calculate the bandwidths */
  std::cout << "epsilon=" << epsilon << std::endl;
  bandwidths.clear();
  inverseBandwidths.clear();
  bestLevelByBandwidth.clear();

  if (bandwidthCount > 1)
  {
    double bandwidthDelta = (highBandwidth - lowBandwidth) / (bandwidthCount - 1);
    for (size_t bIndex = 0; bIndex < bandwidthCount; ++bIndex)
    {
      bandwidths.push_back(lowBandwidth + bandwidthDelta * bIndex);
      inverseBandwidths.push_back(1.0 / bandwidths.back());
      bestLevelByBandwidth.push_back(-1);
    }
  }
  else
  {
    bandwidths.push_back(lowBandwidth);
    inverseBandwidths.push_back(1.0 / lowBandwidth);
    bestLevelByBandwidth.push_back(-1);
  }

  /* resize the critical matrices */
  upperBoundLevelByBandwidth.zeros(levelsInTree,bandwidthCount);
  lowerBoundLevelByBandwidth.zeros(levelsInTree,bandwidthCount);
  /* place pretend log(0)s into the matrix */
  for (size_t bIndex = 0; bIndex < bandwidthCount; ++bIndex)
  {
    arma::vec col = upperBoundLevelByBandwidth.unsafe_col(bIndex);
    arma::vec col2 = lowerBoundLevelByBandwidth.unsafe_col(bIndex);
    col.fill(queryRoot->count() * log(inverseBandwidths[bIndex]));
    col2.fill(queryRoot->count() *
                           (log(DBL_EPSILON) + log(inverseBandwidths[bIndex])));
  }

  upperBoundQPointByBandwidth.zeros(queryRoot->count(),bandwidthCount);
  for (size_t bIndex = 0; bIndex < bandwidthCount; ++bIndex)
  {
    arma::vec col = upperBoundQPointByBandwidth.unsafe_col(bIndex);
    col.fill(referenceRoot->count() * inverseBandwidths[bIndex]);
  }
  lowerBoundQPointByBandwidth.zeros(queryRoot->count(),bandwidthCount);
  upperBoundQNodeByBandwidth.zeros(queryTreeSize,bandwidthCount);
  for (size_t bIndex = 0; bIndex < bandwidthCount; ++bIndex)
  {
    arma::vec col = upperBoundQNodeByBandwidth.unsafe_col(bIndex);
    col.fill(referenceRoot->count() * inverseBandwidths[bIndex]);
  }
  lowerBoundQNodeByBandwidth.zeros(queryTreeSize,bandwidthCount);

  arma::vec dl;
  arma::vec du(bandwidthCount);
  dl.zeros(bandwidthCount);
  for (size_t bIndex = 0; bIndex < bandwidthCount; ++bIndex)
  {
    du(bIndex) = referenceRoot->count() * inverseBandwidths[bIndex];
  }
  struct queueNode<TTree> firstNode =
      {referenceRoot,queryRoot, nextAvailableNodeIndex, dl, du,
        Priority(queryRoot, referenceRoot), 0, bandwidthCount - 1, 0};
  nodeIndices[queryRoot] = nextAvailableNodeIndex;
  ++nextAvailableNodeIndex;
  nodePriorityQueue.push(firstNode);
  size_t finalLevel = MultiBandwidthDualTree();
  std::cout << "the best level is " << finalLevel << "\n";

  size_t maxIndex = -1;
  double maxLogLikelihood = -DBL_MAX;

  for (size_t bIndex = 0; bIndex < bandwidthCount; ++bIndex)
  {
    double currentLogLikelihood = (upperBoundLevelByBandwidth(
                                     bestLevelByBandwidth[bIndex],bIndex) +
                                   lowerBoundLevelByBandwidth(
                                     bestLevelByBandwidth[bIndex],bIndex)) / 2.0;
    std::cout << bandwidths[bIndex] << "," << inverseBandwidths[bIndex] << "," << currentLogLikelihood << "; RANGE " << lowerBoundLevelByBandwidth(bestLevelByBandwidth[bIndex],bIndex) << ", " << upperBoundLevelByBandwidth(bestLevelByBandwidth[bIndex],bIndex) << std::endl;
    if (currentLogLikelihood > maxLogLikelihood)
    {
      maxLogLikelihood = currentLogLikelihood;
      maxIndex = bIndex;
    }
  }
  if (maxIndex == (size_t)-1)
  {
    std::cout << "We failed.\n";
  }
  //std::cout << "upperBoundLevelByBandwidth" << "\n";
  //std::cout << upperBoundLevelByBandwidth << "\n";
  //std::cout << "lowerBoundLevelByBandwidth" << "\n";
  //std::cout << lowerBoundLevelByBandwidth << "\n";
  //std::cout << "upperBoundQNodeByBandwidth" << "\n";
  //std::cout << upperBoundQNodeByBandwidth << "\n";
  //std::cout << "lowerBoundQNodeByBandwidth" << "\n";
  //std::cout << lowerBoundQNodeByBandwidth << "\n";
  std::cout << "best bandwidth " << bandwidths[maxIndex] << ";\n";
  exit(1);
  std::vector<double> densities;
  for (std::vector<size_t>::iterator shuffIt = queryShuffledIndices.begin();
      shuffIt != queryShuffledIndices.end(); ++shuffIt)
  {
    densities.push_back((upperBoundQPointByBandwidth(*shuffIt, maxIndex) +
                         lowerBoundQPointByBandwidth(*shuffIt, maxIndex)) / (kernel.Normalizer() * 2.0 * referenceRoot->count()));

  }
  return densities;
}

template<typename TKernel, typename TTree>
size_t KdeDualTree<TKernel, TTree>::MultiBandwidthDualTree()
{
  /* current level */
  size_t v = 0;
  std::cout << "levels = " << queryRoot->levelsBelow() << std::endl;
  std::cout << "treeSize = " << queryRoot->treeSize() << std::endl;
  while (!nodePriorityQueue.empty())
  {
    /* get the first structure in the queue */
    struct queueNode<TTree> queueCurrent = nodePriorityQueue.top();
    nodePriorityQueue.pop();
    TTree* Q = queueCurrent.Q;
    TTree* T = queueCurrent.T;
    size_t sizeOfTNode = T->count();
    size_t sizeOfQNode = Q->count();
    size_t QIndex = queueCurrent.QIndex;
    arma::vec deltaLower = queueCurrent.deltaLower;
    arma::vec deltaUpper = queueCurrent.deltaUpper;
    /* v is the level of the Q node */
    v = queueCurrent.QLevel;
    //std::cout << v << ":: range " << lowerBoundLevelByBandwidth(v,0) << ", " << upperBoundLevelByBandwidth(v,0) << std::endl;
    size_t bUpper = queueCurrent.bUpperIndex;
    size_t bLower = queueCurrent.bLowerIndex;
    /* check to see whether we've reached the epsilon condition */
    bool epsilonCondition = true;
    for (size_t bIndex = queueCurrent.bLowerIndex;
         bIndex <= queueCurrent.bUpperIndex;
         ++bIndex)
    {
      double constraint = fabs((upperBoundLevelByBandwidth(v,bIndex) -
                           lowerBoundLevelByBandwidth(v,bIndex)) /
                           lowerBoundLevelByBandwidth(v,bIndex));
      if (constraint >= epsilon)
      {
        epsilonCondition = false;
        break;
      }
      else
      {
        if (bestLevelByBandwidth[bIndex] == (size_t)-1)
        {
          bestLevelByBandwidth[bIndex] = v;
        }
      }
    }
    /* return */
    if (epsilonCondition)
    {
      return v;
    }
    /* we didn't meet the criteria; let's narrow the bandwidth range */
    Winnow(v, &bLower, &bUpper);
    if (queueCurrent.priority < PRIORITY_MAX)
    {
      double dMin = pow(Q->bound().MinDistance(T->bound()), 0.5);
      double dMax = pow(Q->bound().MaxDistance(T->bound()), 0.5);
      /* iterate through the remaining bandwidths */
      bool meetsDeltaCondition = true;
      std::vector<bool> deltaCondition;
      for (size_t bIndex = bLower; bIndex <= bUpper; ++bIndex)
      {
        double inverseBandwidth = inverseBandwidths[bIndex];
        double dl = sizeOfTNode * inverseBandwidth * kernel.Evaluate(dMax * inverseBandwidth);
        double du = sizeOfTNode * inverseBandwidth * kernel.Evaluate(dMin * inverseBandwidth);
        deltaLower(bIndex) = dl;
        deltaUpper(bIndex) = du - inverseBandwidth * sizeOfTNode;
        if (fabs((du - dl)/(lowerBoundQNodeByBandwidth(QIndex, bIndex) + dl)) < delta)
        {
          for (size_t q = Q->begin(); q < Q->end(); ++q)
          {
            lowerBoundQPointByBandwidth(q,bIndex) += deltaLower(bIndex);
            upperBoundQPointByBandwidth(q,bIndex) += deltaUpper(bIndex);
          }
          /* subtract the current log-likelihood */
          upperBoundLevelByBandwidth(v, bIndex) -=
              sizeOfQNode * log(upperBoundQNodeByBandwidth(QIndex, bIndex)/ referenceRoot->count());
          double value = lowerBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count();
          if (value > DBL_EPSILON)
          {
            lowerBoundLevelByBandwidth(v, bIndex) -=
                sizeOfQNode * log(value);
          }
          else
          {
            lowerBoundLevelByBandwidth(v, bIndex) -=
                sizeOfQNode * (log(DBL_EPSILON) - log(referenceRoot->count()));
          }
          /* adjust the current inner portion */
          lowerBoundQNodeByBandwidth(QIndex, bIndex) += deltaLower(bIndex);
          upperBoundQNodeByBandwidth(QIndex, bIndex) += deltaUpper(bIndex);
          /* add the corrected log-likelihood */
          upperBoundLevelByBandwidth(v, bIndex) +=
              sizeOfQNode * log(upperBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count());
          value = lowerBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count();
          if (value > DBL_EPSILON)
          {
            lowerBoundLevelByBandwidth(v, bIndex) +=
                sizeOfQNode * log(value);
          }
          else
          {
            lowerBoundLevelByBandwidth(v, bIndex) +=
                sizeOfQNode * (log(DBL_EPSILON) - log(referenceRoot->count()));
          }
        }
        /* check the delta condition with the new values */
        if (fabs((du - dl)/(lowerBoundQNodeByBandwidth(QIndex, bIndex) + dl)) >= delta)
        {
          deltaCondition.push_back(false);
          meetsDeltaCondition = false;
        }
        else
        {
          deltaCondition.push_back(true);
        }
      }
      /* check whether we met the delta condition for
       *   all applicable bandwidths */
      if (meetsDeltaCondition)
      {
        /* adjust the current structure, then reinsert it into the queue */
        queueCurrent.deltaLower = deltaLower;
        queueCurrent.deltaUpper = deltaUpper;
        queueCurrent.bUpperIndex = bUpper;
        queueCurrent.bLowerIndex = bLower;
        queueCurrent.priority = Priority(Q,T) + PRIORITY_MAX;
        nodePriorityQueue.push(queueCurrent);
        /* the continue forces us to undo the previous node eventually
         *   if we don't escape first */
        continue;
      }
      else
      {
        /* winnow according to the delta conditions */
        std::vector<bool>::iterator bIt = deltaCondition.begin();
        while (*bIt && bIt != deltaCondition.end())
        {
          ++bIt;
          //bestLevelByBandwidth[bLower] = v;
          ++bLower;
        }
        bIt = deltaCondition.end();
        --bIt;
        while (*bIt && bIt != deltaCondition.begin())
        {
          --bIt;
          //bestLevelByBandwidth[bUpper] = v;
          --bUpper;
        }
      }
    }
    else /* the priority exceeds the maximum available; back the node out */
    {
      deltaLower = -deltaLower;
      deltaUpper = -deltaUpper;
      for (size_t bIndex = bLower; bIndex <= bUpper; ++bIndex)
      {
        for (size_t q = Q->begin(); q < Q->end(); ++q)
        {
          lowerBoundQPointByBandwidth(q,bIndex) += deltaLower(bIndex);
          upperBoundQPointByBandwidth(q,bIndex) += deltaUpper(bIndex);
        }
        /* subtract the current log-likelihood */
        upperBoundLevelByBandwidth(v, bIndex) -=
            sizeOfQNode * log(upperBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count());
        double value = lowerBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count();
        if (value > DBL_EPSILON)
        {
          lowerBoundLevelByBandwidth(v, bIndex) -=
              sizeOfQNode * log(value);
        }
        else
        {
          lowerBoundLevelByBandwidth(v, bIndex) -=
              sizeOfQNode * (log(DBL_EPSILON) - log(referenceRoot->count()));
        }
        /* adjust the current inner portion */
        lowerBoundQNodeByBandwidth(QIndex, bIndex) += deltaLower(bIndex);
        upperBoundQNodeByBandwidth(QIndex, bIndex) += deltaUpper(bIndex);
        /* add the corrected log-likelihood */
        upperBoundLevelByBandwidth(v, bIndex) +=
            sizeOfQNode * log(upperBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count());
        value = lowerBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count();
        if (lowerBoundQNodeByBandwidth(QIndex, bIndex) > DBL_EPSILON)
        {
          lowerBoundLevelByBandwidth(v, bIndex) +=
              sizeOfQNode * log(value);
        }
        else
        {
          lowerBoundLevelByBandwidth(v, bIndex) +=
              sizeOfQNode * (log(DBL_EPSILON) - log(referenceRoot->count()));
        }
      }
    }
    if (Q->is_leaf() && T->is_leaf())
    {
      MultiBandwidthDualTreeBase(Q, T, QIndex, bLower, bUpper, queueCurrent.QLevel);
    }
    else
    {
      /* we must manipulate the leaf nodes since the tree levels might not match */
      double priority = Priority(Q,T);
      //std::cout << "QIndex for the current non-leaf : " << QIndex << std::endl;
      TTree* QLeft = Q->left();
      TTree* QRight = Q->right();
      size_t QLeftIndex = -1;
      size_t QRightIndex = -1;
      size_t allOrNode = 0;
      if (QLeft)
      {
        if (nodeIndices.find(QLeft) == nodeIndices.end())
        {
          nodeIndices[QLeft] = nextAvailableNodeIndex;
          ++nextAvailableNodeIndex;
        }
        QLeftIndex = (*(nodeIndices.find(QLeft))).second;
      }
      if (QRight)
      {
        if (nodeIndices.find(QRight) == nodeIndices.end())
        {
          nodeIndices[QRight] = nextAvailableNodeIndex;
          ++nextAvailableNodeIndex;
        }
        QRightIndex = (*(nodeIndices.find(QRight))).second;
      }
      arma::vec leftLeftLower(deltaLower.n_rows);
      arma::vec leftLeftUpper(deltaUpper.n_rows);
      arma::vec leftRightLower(deltaLower.n_rows);
      arma::vec leftRightUpper(deltaUpper.n_rows);
      arma::vec rightLeftLower(deltaLower.n_rows);
      arma::vec rightLeftUpper(deltaUpper.n_rows);
      arma::vec rightRightLower(deltaLower.n_rows);
      arma::vec rightRightUpper(deltaUpper.n_rows);
      for (size_t index = 0; index < deltaLower.n_cols; ++index)
      {
        leftLeftLower(index) = deltaLower(index);
        leftLeftUpper(index) = deltaUpper(index);
        leftRightLower(index) = deltaLower(index);
        leftRightUpper(index) = deltaUpper(index);
        rightLeftLower(index) = deltaLower(index);
        rightLeftUpper(index) = deltaUpper(index);
        rightRightLower(index) = deltaLower(index);
        rightRightUpper(index) = deltaUpper(index);
      }
      //std::cout << "deltaUpper address " << &deltaUpper << "\n";
      //std::cout << "leftLeftUpper address " << &leftLeftUpper << "\n";
      /* TODO TODO: we need to figure out node to node comparisons when one is a leaf and the other isn't */
      if (Q->is_leaf() && !T->is_leaf())
      {
        std::cout << "Q is a leaf, T is NOT\n";
      }
      if (!Q->is_leaf() && T->is_leaf())
      {
        std::cout << "T is a leaf, Q is NOT\n";
      }

      if (Q->left() && T->left())
      {
        struct queueNode<TTree> leftLeft =
        {T->left(),Q->left(), QLeftIndex, leftLeftLower,
          leftLeftUpper, priority, bLower, bUpper, queueCurrent.QLevel + 1};
        nodePriorityQueue.push(leftLeft);
        ++allOrNode;
      }
      if (Q->left() && T->right())
      {
        struct queueNode<TTree> leftRight =
        {T->left(),Q->right(), QRightIndex, leftRightLower,
          leftRightUpper, priority, bLower, bUpper, queueCurrent.QLevel + 1};
        nodePriorityQueue.push(leftRight);
        ++allOrNode;
      }
      if (Q->right() && T->left())
      {
        struct queueNode<TTree> rightLeft =
        {T->right(),Q->left(), QLeftIndex, rightLeftLower,
          rightLeftUpper, priority, bLower, bUpper, queueCurrent.QLevel + 1};
        nodePriorityQueue.push(rightLeft);
        ++allOrNode;
      }
      if (Q->right() && T->right())
      {
        struct queueNode<TTree> rightRight =
        {T->right(),Q->right(), QRightIndex, rightRightLower,
          rightRightUpper, priority, bLower, bUpper, queueCurrent.QLevel + 1};
        nodePriorityQueue.push(rightRight);
        ++allOrNode;
      }
      if (allOrNode != 0 && allOrNode != 4)
      {
        std::cout << "unbalanced tree\n";
      }
    }
  }
  MADEIT;
  MADEIT;
  MADEIT;
  return v;
}

template<typename TKernel, typename TTree>
void KdeDualTree<TKernel, TTree>::Winnow(size_t level,
                                         size_t* bLower,
                                         size_t* bUpper)
{
  size_t bIndex = *bLower;
  double constraint = epsilon;
  bool enteredTheLoop = false;
  /* bring the lower up */
  constraint = fabs((upperBoundLevelByBandwidth(level,bIndex) -
                lowerBoundLevelByBandwidth(level,bIndex)) /
                lowerBoundLevelByBandwidth(level,bIndex));
  while (constraint < epsilon && bIndex <= *bUpper)
  {
    enteredTheLoop = true;
    bestLevelByBandwidth[bIndex] = level;
    ++bIndex;
    if (bIndex > *bUpper)
    {
      break;
    }
    constraint = fabs((upperBoundLevelByBandwidth(level,bIndex) -
                  lowerBoundLevelByBandwidth(level,bIndex)) /
                  lowerBoundLevelByBandwidth(level,bIndex));
  }
  if (enteredTheLoop)
  {
    *bLower = bIndex - 1;
  }

  bIndex = *bUpper;
  constraint = epsilon;
  enteredTheLoop = false;
  /* bring the lower up */
  constraint = fabs((upperBoundLevelByBandwidth(level,bIndex) -
                lowerBoundLevelByBandwidth(level,bIndex)) /
                lowerBoundLevelByBandwidth(level,bIndex));
  while (constraint < epsilon && bIndex >= *bLower)
  {
    enteredTheLoop = true;
    bestLevelByBandwidth[bIndex] = level;
    --bIndex;
    if (bIndex < *bUpper)
    {
      break;
    }
    constraint = fabs((upperBoundLevelByBandwidth(level,bIndex) -
                  lowerBoundLevelByBandwidth(level,bIndex)) /
                  lowerBoundLevelByBandwidth(level,bIndex));
  }
  if (enteredTheLoop)
  {
    *bUpper = bIndex + 1;
  }
}


template<typename TKernel, typename TTree>
void KdeDualTree<TKernel, TTree>::MultiBandwidthDualTreeBase(TTree* Q,
                                TTree* T, size_t QIndex,
                                size_t lowerBIndex, size_t upperBIndex, size_t levelOfQ)
{
  size_t sizeOfTNode = T->count();
  size_t sizeOfQNode = Q->count();
  for (size_t q = Q->begin(); q < Q->end(); ++q)
  {
    arma::vec queryPoint = queryData.unsafe_col(q);
    for (size_t t = T->begin(); t < T->end(); ++t)
    {
      arma::vec diff = queryPoint - referenceData.unsafe_col(t);
      double dist = pow(arma::dot(diff, diff), 0.5);
      size_t bandwidthIndex = upperBIndex + 1;
      while (bandwidthIndex > lowerBIndex)
      {
        --bandwidthIndex;
        double inverseBandwidth = inverseBandwidths[bandwidthIndex];
        double scaledProduct = dist * inverseBandwidth;
        /* TODO: determine the power of the incoming argument */
        double contribution = inverseBandwidth * kernel.Evaluate(scaledProduct);
        if (contribution > DBL_EPSILON)
        {
          upperBoundQPointByBandwidth(q, bandwidthIndex) += contribution;
          lowerBoundQPointByBandwidth(q, bandwidthIndex) += contribution;
        }
        else
        {
          break;
        }
      }
    }
    for (size_t bIndex = lowerBIndex; bIndex <= upperBIndex; ++bIndex)
    {
      upperBoundQPointByBandwidth(q, bIndex) -= inverseBandwidths[bIndex] * sizeOfTNode;
    }
  }
  for (size_t bIndex = lowerBIndex; bIndex <= upperBIndex; ++bIndex)
  {
    /* subtract out the current log-likelihood amount for this Q node so we can readjust
     *   the Q node bounds by the current bandwidth */
    upperBoundLevelByBandwidth(levelOfQ, bIndex) -=
        sizeOfQNode * log(upperBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count());
    double value = lowerBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count();
    if (value > DBL_EPSILON)
    {
      lowerBoundLevelByBandwidth(levelOfQ, bIndex) -=
          sizeOfQNode * log(value);
    }
    else
    {
      lowerBoundLevelByBandwidth(levelOfQ, bIndex) -=
          sizeOfQNode * (log(DBL_EPSILON) - log(referenceRoot->count()));
    }
    arma::vec upperBound = upperBoundQPointByBandwidth.unsafe_col(bIndex);
    arma::vec lowerBound = lowerBoundQPointByBandwidth.unsafe_col(bIndex);
    double minimumLower = lowerBoundQPointByBandwidth(Q->begin(), bIndex);
    double maximumUpper = upperBoundQPointByBandwidth(Q->begin(), bIndex);
    for (size_t q = Q->begin(); q < Q->end(); ++q)
    {
      if (lowerBoundQPointByBandwidth(q,bIndex) < minimumLower)
      {
        minimumLower = lowerBoundQPointByBandwidth(q,bIndex);
      }
      if (upperBoundQPointByBandwidth(q,bIndex) > maximumUpper)
      {
        maximumUpper = upperBoundQPointByBandwidth(q,bIndex);
      }
    }
    /* adjust Q node bounds, then add the new quantities to the level by bandwidth
     *   log-likelihood bounds */
    lowerBoundQNodeByBandwidth(QIndex, bIndex) = minimumLower;
    //std::cout << "maximum upper - TNodeSize = " << maximumUpper - sizeOfTNode << "\n";
    upperBoundQNodeByBandwidth(QIndex, bIndex) = maximumUpper;/* -
                                   inverseBandwidths[bIndex] * sizeOfTNode;*/
    upperBoundLevelByBandwidth(levelOfQ, bIndex) +=
        sizeOfQNode * log(upperBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count());
    value = lowerBoundQNodeByBandwidth(QIndex, bIndex) / referenceRoot->count();
    if (value > DBL_EPSILON)
    {
      lowerBoundLevelByBandwidth(levelOfQ, bIndex) +=
          sizeOfQNode * log(value);
    }
    else
    {
      lowerBoundLevelByBandwidth(levelOfQ, bIndex) +=
          sizeOfQNode * (log(DBL_EPSILON) - log(referenceRoot->count()));
    }
  }
}
template<typename TKernel, typename TTree>
void KdeDualTree<TKernel, TTree>::SetBandwidthBounds(double l, double u)
{
  if (u <= l + DBL_EPSILON || l <= DBL_EPSILON)
  {
    Log::Fatal << "Incorrect bandwidth range assignment" << std::endl;
  }
  lowBandwidth = l;
  highBandwidth = u;
}

template<typename TKernel, typename TTree>
double KdeDualTree<TKernel, TTree>::Priority(TTree* Q, TTree* T)
{
  return pow(Q->bound().MinDistance(T->bound()), 0.5);
}
};
};

#endif