/*
Hello!

This program was written by Quinn Desmond, using the SFML library (version 2.5.1) for optionally drawing the output.

The bulk of the logic is inside the step() function, which should be simple to port to another language if necessary.

The program allows one to see the convex hull as it forms, as it will display each pair of points.
It highlights the current pair, the line it creates, and displays all other points as different colors depending on whether they are to the left or right of the line.

The visual output, as well as a handful of other features, make use of the SFML library.

If you wish to compile the program, you will either need to use said library (I would recommend following the guide here: https://www.sfml-dev.org/tutorials/2.5/start-vc.php)
or you will need to set the following define to 0.

(Note that the output point list works regardless of whether SFML is used!)
*/

#define USE_SFML 1

/*
If SFML is enabled, you can use P to reset the input with a new set of points, and Q to take a screenshot (which will be saved as "result.png" in the program directory).
*/

#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>

#if USE_SFML == 1
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#endif

/*
The bulk of the logic is inside the step() function, which should be simple to port to another language if necessary.

The program allows one to see the convex hull as it forms, showing the gradual refinement of the hull as it progresses.
It highlights the current pair, the line it creates, and the current farthest point

The code itself is commentated, but for simple adjustments, you can change the values of the variables below.

randSeed: A number used for generating random points, passed into the srand function. Set to 0 to have it generate a completely random input every time it's run.

stepTimeMS: The amount of time to wait between each step, in milliseconds. The higher this number is, the longer the wait between steps. This is good for if you need a closer look at each individual step.
Set this number to a lower one if you just wish to see the result.

pointCount: The amount of points to create, randomize, and make a hull for. Since Quickhull is O(n log n), you can go much higher with this one.
At 1 MS step time, 4000 points takes about half a second and 400000 takes about 5, so it's very easy to get to a point where the main bottleneck is the poorly-optimized drawing code

windowWidth/windowHeight: The size of the displayed window. Points are evenly distrubited within the window range, with a bit of a buffer given on the edges.

Do note that as pointCount gets higher, the random distribution of a rectangle will make it more and more likely that the convex hull will just be 4 points, that being the corners of the rectangle.
Also note that due to having to store a bunch of data to do the recursion in steps, the amount of memory taken up can increase drastically.
You won't have memory issues with sane numbers, but anything in the millions range can start causing problems. 40,000,000 points can take up 1.5 GB of memory at max.
(This is mainly due to the visualizer, as a lot of state information has to be stored between steps.)
And, with this being a 32-bit program, it's possible to just crash it completely if you try to go too high.
So it's probably best to keep the number low-ish. Things stop being meaningfully visible at high enough numbers, regardless.
(And even if not, there's room to optimize. Converting most of the Points and Point vectors to use pointers instead of copy-by-value could theoretically cut the cost in half, but I don't believe it to be necessary for this scale.)
*/

const int randSeed = 1;

const int stepTimeMS = 300;

const int pointCount = 1000;
const int windowWidth = 1280;
const int windowHeight = 720;
const int windowMargin = 10;
const int pointXmax = windowWidth - (windowMargin * 2), pointYmax = windowHeight - (windowMargin * 2);


//simple point structure. x and y coordinate.
struct Point {
	int x;
	int y;
};

//Stores progress for recursions, since they have multiple steps and are interrupted as such.
//FirstIteration is only use for the very first line.
enum StepDataProgress {
	SDP_RecurseOne,
	SDP_RecurseTwo,
	SDP_Done,
	SDP_FirstIteration
};

//complex structure that contains all the data needed to execute one step of quickhull and setup for the next step.
struct StepData {
	std::vector<Point> pointSet; //points allocated from the previous step's S1 or S2
	Point segmentA, segmentB;
	StepDataProgress progress;

	//pointers to the left half, right half, and parent data, to emulate recursion properly
	std::shared_ptr<StepData> recursiveOne, recursiveTwo, prevStep;
};

class QuickHull {
private:
	std::vector<Point> basePointList;
	std::vector<Point> hullPoints;

	std::shared_ptr<StepData> nextStep;

	//average of min and max, used for calculating point order counter-clockwise
	Point center;

	//mostly stores the location of important points used for drawing.
	Point minPoint, maxPoint, furthestStore;

#if USE_SFML == 1
	//shape presets used for drawing.
	sf::CircleShape mainPoint;
	sf::CircleShape secondaryPoint;
	sf::CircleShape furthestPoint;
#endif

public:
	QuickHull() {
#if USE_SFML == 1
		//create circle shapes used for drawing, their colors, and then center them
		mainPoint = sf::CircleShape(6, 32);
		secondaryPoint = sf::CircleShape(6, 32);
		furthestPoint = sf::CircleShape(6, 32);

		mainPoint.setFillColor(sf::Color(0xFF0000FF));
		secondaryPoint.setFillColor(sf::Color(0x3F3F3FFF));
		furthestPoint.setFillColor(sf::Color(0x00FF00FF));

		mainPoint.setOrigin(6, 6);
		secondaryPoint.setOrigin(6, 6);
		furthestPoint.setOrigin(6, 6);
#endif
	}

	void randomizeInput(int pointCount) {
		//clear out lists of points from previous input set
		basePointList.clear();
		hullPoints.clear();

		//creates a certain amount of points with random locations inside the window boundary
		for (int i = 0; i < pointCount; i++) {
			int x = rand() % pointXmax + windowMargin;
			int y = rand() % pointYmax + windowMargin;

			Point newPoint;
			newPoint.x = x;
			newPoint.y = y;
			basePointList.push_back(std::move(newPoint));
		}

		//sorts points from left-to-right, top-to-bottom
		std::sort(basePointList.begin(), basePointList.end(), [](const Point& left, const Point& right) {
			return (left.x < right.x) || (left.x == right.x && left.y < right.y);
			});

		//sets up some initial values
		minPoint = basePointList[0];
		maxPoint = basePointList[basePointList.size() - 1];

		//creates first step with a full point list, and manually sets up its recursion steps
		nextStep = std::make_shared<StepData>();
		nextStep->pointSet = basePointList;
		nextStep->progress = SDP_FirstIteration;
		nextStep->segmentA = minPoint;
		nextStep->segmentB = maxPoint;
		prepareNextRecursion(nextStep, minPoint, minPoint, maxPoint);

		//add first two points to the hull list
		hullPoints.push_back(minPoint);
		hullPoints.push_back(maxPoint);

		center.x = windowWidth / 2;
		center.y = windowHeight / 2;
	}

	bool comparePoints(const Point lhs, const Point rhs) {
		return lhs.x == rhs.x && lhs.y == rhs.y;
	}

	std::vector<Point> calcPointsOnRightSide(Point begin, Point end, const std::vector<Point>& list) {
		std::vector<Point> temp;
		for (int x = 0; x < list.size(); x++) {
			if (comparePoints(list[x], begin) || comparePoints(list[x], end)) {
				continue;
			}
			//go through all points that aren't part of the current line and determine their side

			int x1 = begin.x;
			int y1 = begin.y;
			int x2 = end.x;
			int y2 = end.y;
			int x3 = list[x].x;
			int y3 = list[x].y;

			//calculate determinant between current point and line points, to determine if on left or right of line
			int determinant = (x1 * y2) + (x3 * y1) + (x2 * y3) - (x3 * y2) - (x2 * y1) - (x1 * y3);

			if (determinant < 0) {
				temp.push_back(list[x]);
			}
		}
		return temp;
	}

	Point calculateFurthestPoint(Point segmentA, Point segmentB, const std::vector<Point>& list) {
		Point furthest = list[0]; //Default value
		int prevMax = -1;
		for (int x = 0; x < list.size(); x++) {
			//go through all points that aren't part of the current line and determine their side

			int x1 = segmentA.x;
			int y1 = segmentA.y;
			int x2 = segmentB.x;
			int y2 = segmentB.y;
			int x3 = list[x].x;
			int y3 = list[x].y;

			//calculate determinant between current point and line points, to determine distance
			int determinant = abs((x1 * y2) + (x3 * y1) + (x2 * y3) - (x3 * y2) - (x2 * y1) - (x1 * y3));

			if (determinant > prevMax) {
				prevMax = determinant;
				furthest = list[x];
			}
		}
		return furthest;
	}

	//Called in step(), prepares the next steps of recursion, including their point fields and such
	void prepareNextRecursion(std::shared_ptr<StepData> currentStep, Point P, Point Q, Point C) {
		//Create step data for both left and right recursion
		std::shared_ptr<StepData> leftStep = std::make_shared<StepData>();
		std::shared_ptr<StepData> rightStep = std::make_shared<StepData>();

		leftStep->progress = SDP_RecurseOne;
		rightStep->progress = SDP_RecurseOne;

		//go through all points and sort into proper sides based off the given lines
		leftStep->pointSet = calcPointsOnRightSide(P, C, currentStep->pointSet);
		rightStep->pointSet = calcPointsOnRightSide(C, Q, currentStep->pointSet);

		//Sort the point lists of the left and right steps by order, left-to-right, top-to-bottom
		std::sort(leftStep->pointSet.begin(), leftStep->pointSet.end(), [](const Point& left, const Point& right) {
			return (left.x < right.x) || (left.x == right.x && left.y < right.y);
			});
		std::sort(rightStep->pointSet.begin(), rightStep->pointSet.end(), [](const Point& left, const Point& right) {
			return (left.x < right.x) || (left.x == right.x && left.y < right.y);
			});

		//Set up split lines
		leftStep->segmentA = P;
		leftStep->segmentB = C;

		rightStep->segmentA = C;
		rightStep->segmentB = Q;

		//Link back for when recursion is done
		leftStep->prevStep = currentStep;
		rightStep->prevStep = currentStep;

		currentStep->recursiveOne = rightStep;
		currentStep->recursiveTwo = leftStep;
	}

	bool step() {
		//this function is very complicated because i had to do a lot of workarounds to make it so that the recursion process could be individually stepped.
		//a more standard c++ implementation would be far simpler, but that's the price you pay for cool visuals, i suppose.

		//if gone through all points, done
		if (nextStep->pointSet.size() == 0) {
			nextStep->progress = SDP_Done;
		}

		//if current node is done, move back until you get one that isn't
		while (nextStep->progress == SDP_Done) {
			//if out of nodes, done forever

			//clean up memory a bit (though this won't happen until a bit after the memory peaks)
			nextStep->recursiveOne = nullptr;
			nextStep->recursiveTwo = nullptr;

			if (nextStep->prevStep == nullptr) {
				return false;
			}
			nextStep = nextStep->prevStep;
		}

		//alias for convenience
		std::vector<Point>& stepPoints = nextStep->pointSet;

		//stepPoints is already sorted so min and max is easy
		minPoint = stepPoints[0];
		maxPoint = stepPoints[stepPoints.size() - 1];

		Point furthest = calculateFurthestPoint(nextStep->segmentA, nextStep->segmentB, stepPoints);

		//Theoretically we could always prepare next recursion instead of just the first time a step is evaluated, but it'd be a waste of computing power to do so.
		if (nextStep->progress == SDP_RecurseOne) {
			prepareNextRecursion(nextStep, nextStep->segmentA, nextStep->segmentB, furthest);
			hullPoints.push_back(furthest);

			furthestStore = furthest;
		}

		//determine the next step based off of recursion progress of current step
		switch (nextStep->progress) {
		case SDP_FirstIteration:
		case SDP_RecurseOne:
			nextStep->progress = SDP_RecurseTwo;
			nextStep = nextStep->recursiveOne;
			break;
		case SDP_RecurseTwo:
			nextStep->progress = SDP_Done;
			nextStep = nextStep->recursiveTwo;
			break;
		}

		return true;
	}

	//uses arctan to find angle
	float calculateAngleFromPoints(Point A, Point B) {
		Point pointDifference;
		pointDifference.x = A.x - B.x;
		pointDifference.y = A.y - B.y;
		double radianAngle = atan2(pointDifference.y, pointDifference.x);
		return (180.f / 3.14159f) * radianAngle;
	}

	std::vector<Point> sortPointsCounterclockwise(std::vector<Point> list, Point center) {
		std::vector<Point> temp = list;
		std::sort(temp.begin(), temp.end(), [&](const Point& left, const Point& right) {
			return calculateAngleFromPoints(center, left) < calculateAngleFromPoints(center, right);
			});

		return temp;
	}

	void outputHullPoints() {
		//Sort points
		std::vector<Point> sortedPoints = sortPointsCounterclockwise(hullPoints, center);

		//Now write to file
		std::ofstream outfile;

		outfile.open("points.txt");

		if (outfile) {
			for (int x = 0; x < sortedPoints.size(); x++) {
				outfile << sortedPoints[x].x << "," << sortedPoints[x].y << '\n';
			}
			outfile.flush();
			outfile.close();
		}
		else {
			std::cout << "Error: Unable to create output file! Is the current folder write-protected?" << std::endl;
		}
	}

#if USE_SFML == 1
	//helper function for drawing a line with a given width and color
	void drawLine(sf::RenderTarget& canvas, Point start, Point end, float lineWidth, sf::Color lineColor) {
		sf::Vector2f difference = sf::Vector2f(end.x - start.x, end.y - start.y);
		float differenceMagnitude = sqrtf(difference.x * difference.x + difference.y * difference.y);
		sf::Vector2f normalized = difference / differenceMagnitude;
		sf::Vector2f lineVisualOffset = sf::Vector2f(-normalized.y, normalized.x) * lineWidth;

		sf::VertexArray lineVisual = sf::VertexArray(sf::Quads, 4);

		lineVisual[0].position = sf::Vector2f(start.x, start.y) + lineVisualOffset;
		lineVisual[1].position = sf::Vector2f(start.x, start.y) - lineVisualOffset;
		lineVisual[2].position = sf::Vector2f(end.x, end.y) - lineVisualOffset;
		lineVisual[3].position = sf::Vector2f(end.x, end.y) + lineVisualOffset;

		lineVisual[0].color = lineColor;
		lineVisual[1].color = lineColor;
		lineVisual[2].color = lineColor;
		lineVisual[3].color = lineColor;

		canvas.draw(lineVisual);
	}

	//draws result to the window
	void render(sf::RenderTarget& canvas) {

		const float lineWidth = 4;

		//get an ordered set of points, and use them to draw lines
		std::vector<Point> sortedPoints = sortPointsCounterclockwise(hullPoints, center);
		for (int x = 0; x < sortedPoints.size() - 1; x++) {
			drawLine(canvas, sortedPoints[x], sortedPoints[x + 1], lineWidth, sf::Color::Black);
		}
		drawLine(canvas, sortedPoints[sortedPoints.size() - 1], sortedPoints[0], lineWidth, sf::Color::Blue);


		//Draw all points
		for (Point p : basePointList) {
			secondaryPoint.setPosition(p.x, p.y);
			canvas.draw(secondaryPoint);
		}

		//now draw the current min and max points over the previous points
		mainPoint.setPosition(minPoint.x, minPoint.y);
		canvas.draw(mainPoint);
		mainPoint.setPosition(maxPoint.x, maxPoint.y);
		canvas.draw(mainPoint);

		furthestPoint.setPosition(furthestStore.x, furthestStore.y);
		canvas.draw(furthestPoint);
	}
#endif
};

int main() {
	//Seed random number generator
	srand(randSeed);

	//Create class to calculate hull
	QuickHull QH = QuickHull();
	QH.randomizeInput(pointCount);

	//Variable to stop updating and re-drawing the points once the hull is complete
	bool continueLoop = true;

#if USE_SFML == 1
	//boilerplate to create window for display
	sf::RenderWindow m_window;
	m_window.create(sf::VideoMode(windowWidth, windowHeight), "Convex Hull QuickHull", sf::Style::Default);

	sf::Event m_event;

	//Main loop
	while (m_window.isOpen()) {
		//Boilerplate that makes window run and resets points if P is pressed
		while (m_window.pollEvent(m_event)) {
			switch (m_event.type) {
			case sf::Event::Closed:
				m_window.close();
				break;
			case sf::Event::KeyPressed:
				if (m_event.key.code == sf::Keyboard::P) {
					QH.randomizeInput(pointCount);
					continueLoop = true;
				}
				if (m_event.key.code == sf::Keyboard::Q) {
					sf::Texture texture;
					texture.create(m_window.getSize().x, m_window.getSize().y);
					texture.update(m_window);
					texture.copyToImage().saveToFile("result.png");
				}
				break;
			}
		}

		if (continueLoop) {
			//Main logic, calculates one pair of points per step
			continueLoop = QH.step();

			//Boilerplate for displaying result
			m_window.clear(sf::Color::White);

			//This line here is what draws everything. Commenting it out will give a ridiculous boost to the algorithm speed (400000 points went from 5 seconds to 0.25), but you can't really see the result that way
			QH.render(m_window);

			m_window.display();

			//Used for making screenshots save without the purple line (I have no idea why it works like this)
			if (!continueLoop) {
				QH.outputHullPoints();
				m_window.display();
			}

			sf::sleep(sf::milliseconds(stepTimeMS));
		}
		else {
			//Just so that it doesn't run at an absurdly high framerate and eat up CPU
			sf::sleep(sf::milliseconds(30));
		}
	}
#else
	while (continueLoop) {
		continueLoop = QH.step();
	}
	QH.outputHullPoints();
#endif

}
