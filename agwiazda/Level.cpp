#include "headers.h"

Level::Level(unsigned int width, unsigned int height)
{
	m_startPosition.x = 0;
	m_startPosition.y = 0;

	m_currentPosition.x = 0;
	m_currentPosition.y = 0;

	m_finishPosition.x = 0;
	m_finishPosition.y = 0;

	m_map = vector<vector<char> >(width, vector<char>(height,Resource::NO_SPRITE));
	m_nodeMap = vector<vector<class Node> >(width, vector<class Node>(height));

	m_drawVisitedNodes = false;
	m_drawVisitedSectors = true;
	m_fillsector = false;

	qt = new class CQuadTree(0.0, 0.0, (float)(width*SQUARE_SIZE), (float)(height*SQUARE_SIZE), 0, 3);

	m_heuristic = eHeuristic::Euclidian;
}

Level::~Level()
{
	m_map.clear();
}

void Level::draw(sf::RenderWindow& wnd, bool renderFinish)
{
	Resource *res = Resource::GetInstance();
	sf::Sprite sprite_to_draw = res->getSprite(Resource::SPR_FINISH);

	if (renderFinish)
	{
		sprite_to_draw.setPosition((float)m_finishPosition.x * SQUARE_SIZE, (float)m_finishPosition.y * SQUARE_SIZE);
		wnd.draw(sprite_to_draw);
	}

	qt->debugDraw(wnd);

	for (size_t x = 0; x < m_map.size(); x++)
	{
		for (size_t y = 0; y < m_map[x].size(); y++)
		{
			if (m_map[x][y] != Resource::NO_SPRITE)
			{
				sprite_to_draw = res->getSprite((Resource::SPRITES)m_map[x][y]);
				sprite_to_draw.setPosition((float)x * SQUARE_SIZE, (float)y * SQUARE_SIZE);

				wnd.draw(sprite_to_draw);
			}
		}
	}

	
}

void Level::drawVisitedNodes(bool enable)
{
	m_drawVisitedNodes = enable;
}

void Level::drawVisitedSectors(bool enable)
{
	m_drawVisitedSectors = enable;
}

bool Level::isDrawingVisitedNodes() const
{
	return m_drawVisitedNodes;
}

bool Level::isDrawingVisitedSectors() const
{
	return m_drawVisitedSectors;
}

void Level::tick(sf::RenderWindow& wnd)
{
	removePathFromMap();

	//A* tutaj
	std::vector<sf::Vector2i> path = calculatePath(m_startPosition.x, m_startPosition.y, m_finishPosition.x, m_finishPosition.y, false);
	//std::reverse(path.begin(), path.end());

	for (unsigned int i = 0; i < path.size(); i++)
	{
		m_map[path[i].x][path[i].y] = Resource::SPR_PATH;



		// pr0 haXor, kolorowanie sektor�w
		if (isDrawingVisitedSectors())
		{
			auto o = new CollisionObject((float)path[i].x * SQUARE_SIZE, (float)path[i].y* SQUARE_SIZE, 10, 10);
			auto w = qt->getSectorAt(o);
			if (w != nullptr)
			{
				w->getSzejp().setFillColor(sf::Color(0, 255, 0, 128));
			}
		}
	}

	

}

void Level::removePathFromMap()
{
	for (size_t x = 0; x < m_map.size(); x++)
	{
		for (size_t y = 0; y < m_map[x].size(); y++)
		{
			if (m_map[x][y] == Resource::SPR_PATH || m_map[x][y] == Resource::SPR_VISITED)
			{
				m_map[x][y] = Resource::NO_SPRITE;
			}
		}
	}
}

std::vector<sf::Vector2i> Level::calculatePath(const int xStart, const int yStart, const int xFinish, const int yFinish, bool allowDiagMovement)
{
	prepareNodeMapForAstar();
	std::vector<sf::Vector2i> path;

	// prepare clock to measure time
	m_timer.restart();

	// Define points to work with
	Node *start = &m_nodeMap[xStart][yStart];
	Node *end = &m_nodeMap[xFinish][yFinish];
	Node *current;
	Node *child;

	// Define the open and the close list
	std::priority_queue<Node*, std::vector<Node*>, Node> openQ;


	// Add the start point to the openList
	openQ.push(start);
	start->opened = true;

	// ilo�� iteracji
	unsigned int n = 0;


	while (n == 0 || (current != end && n < 128*72) )
	{

		// Look for the smallest F value in the openList and make it the current point
		if (openQ.empty())
			break;

		current = openQ.top();

		// Stop if we reached the end
		if (current == end)
		{
			break;
		}

		// Remove the current point from the openList
		openQ.pop();
		current->opened = false;

		// Add the current point to the closedList
		current->closed = true;

		// Get all current's adjacent walkable points (inc diagonal)
		for (int x = -1; x < 2; x++)
		{
			for (int y = -1; y < 2; y++)
			{
				// If it's current point then pass
				if (x == 0 && y == 0)
				{
					continue;
				}

				// if we're not allowing diaganal movement then only one of x or y can be set
				if (!allowDiagMovement)
				{
					if ((x != 0) && (y != 0)) 
					{
						continue;
					}
				}


				// Get this point
				int xp = x + current->xPos;
				int yp = y + current->yPos;
			

				if (isValidLocation(xp, yp))
				{
					// Get this point
					child = &m_nodeMap[xp][yp];

					//oznaczamy �e odwiedzili�my ten kawa�ek
					if (isDrawingVisitedNodes())
					{
						m_map[xp][yp] = Resource::SPR_VISITED;
					}

					// If it's closed or not walkable then pass
					if (child->closed || !child->walkable)
					{
						continue;
					}


					// If it's already in the openList
					if (child->opened)
					{
						// If it has a wroste g score than the one that pass through the current point
						// then its path is improved when it's parent is the current point
						if (child->g > child->getGScore(current))
						{
							// Change its parent and g score
							child->parent = current;
							child->computeScores(end, getHeuristic());
						}
					}
					else
					{
						// Add it to the openList with current point as parent
						child->opened = true;

						// Compute it's g, h and f score
						child->parent = current;
						child->computeScores(end, getHeuristic());

						openQ.push(child);
					}

				} // if validLocation

			} // end for
		} // end for
		n++;
	} // end while

	while (current->hasParent() && current != start)
	{
		path.push_back(sf::Vector2i(current->xPos, current->yPos));
		current = current->parent;
	}

	//save time elapsed
	m_calculateTime = m_timer.getElapsedTime();

	return path;
}

void Level::prepareNodeMapForAstar()
{

	for (size_t x = 0; x < m_map.size(); x++)
	{
		for (size_t y = 0; y < m_map[x].size(); y++)
		{
			m_nodeMap[x][y].xPos = x;
			m_nodeMap[x][y].yPos = y;

			switch ( m_map[x][y] )
			{
				case Resource::NO_SPRITE:
				{
					m_nodeMap[x][y].walkable = true;
					break;
				}
				case Resource::SPR_COLLIDER:
				{
					m_nodeMap[x][y].walkable = false;
					break;
				}
				case Resource::SPR_PATH:
				{
					m_nodeMap[x][y].walkable = true;
					break;
				}
				case Resource::SPR_START:
				{
					m_nodeMap[x][y].walkable = true;
					break;
				}
				case Resource::SPR_FINISH:
				{
					m_nodeMap[x][y].walkable = true;
					break;
				}
			}
		}
	}
}

bool Level::isValidLocation(int x, int y)
{
	// czy pozycja miesci sie w wymiarach mapy
	bool invalid = (x<0) || (y<0) || ((unsigned int)x >= m_map.size()) || ((unsigned int)y >= m_map[0].size());

	// jak sie miesci to sprawdz czy nie jest zablokowana pozycja
	if ((!invalid))
	{
		if (m_map[x][y] == Resource::SPR_COLLIDER)
		{
			invalid = true;
		}
	}

	return !invalid;
}

void Level::setStartPosition(int x, int y)
{
	m_startPosition.x = x;
	m_startPosition.y = y;
	m_map[x][y] = Resource::SPR_START;
	qt->addObject(new CollisionObject((float)x, (float)y, 1.0, 1.0, CollisionObject::START));
}

void Level::setFinishPosition(int x, int y)
{
	m_finishPosition.x = x;
	m_finishPosition.y = y;
	m_map[x][y] = Resource::SPR_FINISH;
	qt->addObject(new CollisionObject((float)x, (float)y, 1.0, 1.0, CollisionObject::FINISH));
}

void Level::setCollider(int x, int y)
{
	if (x < 0 || y < 0 || x >= (int)m_map.size() || y >= (int)m_map[x].size()) return;

	if (m_map[x][y] == Resource::NO_SPRITE && (x != m_finishPosition.x || y != m_finishPosition.y))
	{
		if (!m_fillsector)
		{
			m_map[x][y] = Resource::SPR_COLLIDER;
			qt->addObject(new CollisionObject((float)x, (float)y, 1.0, 1.0, CollisionObject::WALL));
		}
		else
		{
			fillSector(x, y);
		}
	}
}

void Level::unsetCollider(int x, int y)
{
	if (x < 0 || y < 0 || x >= (int)m_map.size() || y >= (int)m_map[x].size()) return;

	if (m_map[x][y] == Resource::SPR_COLLIDER && (x != m_finishPosition.x || y != m_finishPosition.y))
	{
		if (!m_fillsector)
		{
			m_map[x][y] = Resource::NO_SPRITE;
		}
		else
		{
			freeSector(x, y);
		}
	}
}

Level::eHeuristic Level::getHeuristic()
{
	return m_heuristic;
}

void Level::changeHeuristic()
{
	m_heuristic = static_cast<eHeuristic>( static_cast<int>(m_heuristic) + 1);
	if (m_heuristic > 2)
	{
		m_heuristic = static_cast<eHeuristic>(0);
	}
}

sf::Time Level::getCalculateTime()
{
	return m_calculateTime;
}

bool Level::isFillingSectors() const
{
	return m_fillsector;
}

void Level::changeSectorFill()
{
	m_fillsector = (m_fillsector) ? false : true;

	if (m_fillsector)
	{
		for (int x = 0; x < 128; x++)
		{
			for (int y = 0; y < 72; y++)
			{
				if (m_map[x][y] == Resource::SPR_COLLIDER)
					fillSector(x, y);
			}
		}
	}
}

void Level::fillSector(int x, int y)
{
	x /= 16;
	y /= 9;
	x *= 16;
	y *= 9;
	for (int x1 = 0; x1 < 16; x1++)
	{
		for (int y1 = 0; y1 < 9; y1++)
		{
			if (m_map[x + x1][y + y1] != Resource::SPR_COLLIDER)
			{
				m_map[x + x1][y + y1] = Resource::SPR_COLLIDER;
				qt->addObject(new CollisionObject((float)(x + x1), (float)(y + y1), 1.0, 1.0, CollisionObject::WALL));
			}
		}
	}
}

void Level::freeSector(int x, int y)
{
	x /= 16;
	y /= 9;
	x *= 16;
	y *= 9;
	for (int x1 = 0; x1 < 16; x1++)
	{
		for (int y1 = 0; y1 < 9; y1++)
		{
			if (m_map[x + x1][y + y1] != Resource::SPR_COLLIDER)
			{
				m_map[x + x1][y + y1] = Resource::NO_SPRITE;
				qt->addObject(new CollisionObject((float)(x + x1), (float)(y + y1), 1.0, 1.0, CollisionObject::WALL));
			}
		}
	}
}

void Level::reset()
{
	m_map[m_startPosition.x][m_startPosition.y] = Resource::NO_SPRITE;
	m_map[m_finishPosition.x][m_finishPosition.y] = Resource::NO_SPRITE;

	for (auto node : m_nodeMap)
		node.clear();
	m_nodeMap.clear();
	m_nodeMap = vector<vector<class Node> >(m_map.size(), vector<class Node>(m_map[0].size()));

	m_drawVisitedNodes = false;
	m_drawVisitedSectors = true;
	m_fillsector = false;
	m_calculateTime = sf::Time::Zero;

	removePathFromMap();
	delete qt;

	qt = new class CQuadTree(0.0, 0.0, (float)(m_map.size()*SQUARE_SIZE), (float)(m_map[0].size()*SQUARE_SIZE), 0, 3);
	for (size_t x = 0; x < m_map.size(); x++)
	{
		for (size_t y = 0; y < m_map[0].size(); y++)
		{
			if (m_map[x][y] == Resource::SPR_COLLIDER)
			{
				qt->addObject(new CollisionObject((float)x, (float)y, 1.0, 1.0, CollisionObject::WALL));
			}
		}
	}
}
