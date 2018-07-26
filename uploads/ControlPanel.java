package code;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.ItemEvent;
import java.awt.event.ItemListener;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.util.Hashtable;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JSlider;
import javax.swing.JTextField;
import javax.swing.SwingConstants;
import javax.swing.ToolTipManager;
import javax.swing.UIManager;
import javax.swing.border.BevelBorder;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;
import javax.swing.text.PlainDocument;

import code.GraphView.Mode;

/**
 * Creates and handles the components on the screen that the user can interact
 * with.
 * 
 * @author Luke
 *
 */
@SuppressWarnings("serial")
public class ControlPanel extends JComponent {

	private SimModel model;
	private GraphView view;
	private SimComponent comp;
	private JLabel currT;
	private JLabel currP;

	// COMPONENTS NEEDED FOR AUTOCARNOT
	private JSlider tempSlider;
	private JButton restart;
	private JButton playPause;
	private JButton moveWallIn;
	private JCheckBox colourParticlesAtActEnergy;
	private JCheckBox insulated;
	private JCheckBox particlesPushWall;
	private JSlider numParticlesSlider;

	// Activation energy components which need to be enabled/disabled
	private JCheckBox particlesDisappearAtActEnergy;
	private JSlider actEnergySlider;
	private JTextField actEnergyValue;
	private JLabel actEnergyLabel;
	// For reset button:
	private JSlider fpsSlider;
	private JComboBox<String> menu;

	// The button to automatically create a Carnot cycle graph
	private JButton autoCarnot;
	// Continuous Carnot cycles
	private JButton autoCarnotCont;
	// Are we running an auto Carnot cycle?
	private boolean running = false;
	// Is the autoCarnot function requesting the restart?
	private boolean carnotRestart = false;
	// Is the simulation paused?
	private boolean pause = false;

	public ControlPanel(Simulation sim, JFrame frame) {
		super();

		this.model = new SimModel(sim);
		createAutoCarnot();
		this.view = new GraphView(model, autoCarnot, autoCarnotCont, this);
		model.addObserver(view);
		// Prepare the help screens (top left and top right buttons)
		HelpScreens help = new HelpScreens(this);

		// Increase the time that the tooltips are displayed before disappearing
		ToolTipManager.sharedInstance().setDismissDelay(15000);
		// Set the default font of checkboxes to Calibri
		UIManager.put("CheckBox.font", "Calibri");

		frame.setLayout(new GridBagLayout());
		GridBagConstraints c = new GridBagConstraints();

		JPanel menuBar = new JPanel(new BorderLayout());
		JPanel UI = new JPanel(new BorderLayout());
		JPanel graphView = new JPanel(new GridBagLayout());

		// Top left drop-down menu
		String[] modes = { "Heat Engines", "Activation Energy" };
		menu = new JComboBox<String>(modes);
		menu.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				String m = (String) menu.getSelectedItem();
				// The user should only be able to interact with Activation
				// Energy components in the activation energy mode
				if (m.equals("Heat Engines")) {
					model.changeMode(Mode.HeatEngines);
					colourParticlesAtActEnergy.setSelected(false);
					colourParticlesAtActEnergy.setEnabled(false);
					particlesDisappearAtActEnergy.setEnabled(false);
					particlesDisappearAtActEnergy.setSelected(false);
					actEnergySlider.setEnabled(false);
					actEnergyValue.setEnabled(false);
					actEnergyLabel.setEnabled(false);
				} else if (m.equals("Activation Energy")) {
					model.changeMode(Mode.ActivationEnergy);
					colourParticlesAtActEnergy.setSelected(true);
					colourParticlesAtActEnergy.setEnabled(true);
					particlesDisappearAtActEnergy.setEnabled(true);
					actEnergySlider.setEnabled(true);
					actEnergyValue.setEnabled(true);
					actEnergyLabel.setEnabled(true);
				}
			}
		});
		menu.setMinimumSize(new Dimension(160, 40));
		// Get the buttons for the top left help screen
		JButton menuHelp = help.createMenuHelp(menu);

		menuBar.add(menu, BorderLayout.CENTER);
		menuBar.add(menuHelp, BorderLayout.EAST);
		// Create the slider bar (bottom centre)
		JPanel sliders = sliderBar();
		// Create the button bar (bottom right)
		JPanel buttons = buttonBar();
		// Start drawing the container and the particles in it
		comp = new SimComponent(model, frame, currT, currP, this);

		UI.add(sliders, BorderLayout.CENTER);
		UI.add(buttons, BorderLayout.EAST);

		// Set the size and position of the graphs and components above it
		view.setMinimumSize(new Dimension(200, frame.getHeight() - 50));
		view.setPreferredSize(new Dimension(200, frame.getHeight()));
		view.setMaximumSize(new Dimension(200, frame.getHeight()));
		c.anchor = GridBagConstraints.FIRST_LINE_START; // Stick to top left
		c.ipadx = 0;
		c.ipady = 0;
		c.weightx = 0.5;
		c.weighty = 0.5;

		graphView.setMinimumSize(new Dimension(277, frame.getHeight()));
		graphView.setPreferredSize(new Dimension(277, frame.getHeight()));

		c.fill = GridBagConstraints.HORIZONTAL;
		c.gridx = 0;
		c.gridy = 0;
		graphView.add(menuBar, c);

		c.fill = GridBagConstraints.BOTH;
		c.gridy = 1;
		graphView.add(view, c);

		c.gridx = 0;
		c.gridy = 0;
		c.gridheight = 2;

		// Draw the graphs on the window
		frame.add(graphView, c);

		// Set the size and position of the container
		Dimension d = new Dimension((int) model.getContainer().getWidth() + 320,
				(int) model.getContainer().getHeight() + 3);
		comp.setMinimumSize(d);
		comp.setPreferredSize(d);
		c.anchor = GridBagConstraints.NORTHWEST;
		c.fill = GridBagConstraints.BOTH;
		c.gridx = 1;
		c.gridy = 0;
		c.gridheight = 1;
		comp.setBorder(BorderFactory.createBevelBorder(BevelBorder.LOWERED));
		// Add the INFO button to the right of the container
		JButton info = help.createInfoButton();
		JPanel compAndInfo = new JPanel(new GridBagLayout());
		GridBagConstraints c2 = new GridBagConstraints();
		c2.anchor = GridBagConstraints.WEST;
		c2.gridx = 0;
		c2.gridy = 0;
		c2.fill = GridBagConstraints.BOTH;
		compAndInfo.add(comp, c2);
		c2.anchor = GridBagConstraints.NORTHEAST;
		c2.gridx = 1;
		c2.fill = GridBagConstraints.HORIZONTAL;
		compAndInfo.add(info, c2);

		// Draw the simulation on the window
		frame.add(compAndInfo, c);

		// Set the position of the bottom bar
		c.fill = GridBagConstraints.HORIZONTAL;
		c.anchor = GridBagConstraints.PAGE_END; // Stick to bottom
		c.ipadx = 20;
		c.ipady = 0;
		c.gridx = 1;
		c.gridy = 1;
		UI.setBorder(BorderFactory.createBevelBorder(BevelBorder.LOWERED));
		// Draw the bottom bar on the window
		frame.add(UI, c);
	}

	/**
	 * @return A JPanel containing all of the components on the slider bar
	 *         (bottom centre)
	 */
	private JPanel sliderBar() {
		// Whole thing
		JPanel sliderBar = new JPanel(new GridLayout(0, 5));
		// Temp slider, label and value
		JPanel tempComp = new JPanel(new BorderLayout());
		// FPS slider, label and value
		JPanel fps = new JPanel(new BorderLayout());
		// Current T and P
		JPanel stats = new JPanel(new BorderLayout());
		// NumParticles slider, label and value
		JPanel numParticles = new JPanel(new BorderLayout());
		// ActivationEnergy slider, label and value
		JPanel actEnergy = new JPanel(new BorderLayout());

		// Create the Number of Particles slider and its surrounding components
		JPanel numParticlesText = new JPanel(new GridLayout(0, 1));
		JPanel numParticlesTextTop = new JPanel();
		JPanel numParticlesTextBot = new JPanel();
		JLabel numParticlesLabel = new JLabel("Number of Particles", SwingConstants.CENTER);
		numParticlesLabel.setFont(new Font("Calibri", Font.PLAIN, 12));
		numParticlesSlider = new JSlider(SwingConstants.HORIZONTAL, 0, 500, 250);
		numParticlesSlider.setMajorTickSpacing(100);
		numParticlesSlider.setMinorTickSpacing(25);
		numParticlesSlider.setPaintTicks(true);
		numParticlesSlider.setPaintLabels(true);
		JTextField numParticlesValue = new JTextField("250");
		// Make sure only integers can be entered into the text field
		PlainDocument numParticlesDoc = (PlainDocument) numParticlesValue.getDocument();
		numParticlesDoc.setDocumentFilter(new IntFilter());

		numParticlesValue.setColumns(3);
		// When the user presses Enter to confirm their input
		numParticlesValue.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				int val = Integer.parseInt(numParticlesValue.getText());
				int max = numParticlesSlider.getMaximum();
				int min = numParticlesSlider.getMinimum();
				// Make sure the new value is a valid one
				if (val > max) {
					val = max;
				}
				if (val < min) {
					val = min;
				}
				numParticlesSlider.setValue(val);
				numParticlesSlider.requestFocus();
				numParticlesValue.setText(Integer.toString(val));
			}
		});
		// Make sure the slider and the text field show the same value
		numParticlesSlider.addChangeListener(new ChangeListener() {
			public void stateChanged(ChangeEvent e) {
				model.setNumParticles(numParticlesSlider.getValue());
				numParticlesValue.setText(Integer.toString(model.getNumParticles()));
			}
		});
		numParticlesTextTop.add(numParticlesLabel);
		numParticlesTextBot.add(numParticlesValue);
		numParticlesText.add(numParticlesTextTop);
		numParticlesText.add(numParticlesTextBot);
		numParticlesText.setPreferredSize(new Dimension(50, 60));
		numParticles.add(numParticlesText, BorderLayout.NORTH);
		numParticles.add(numParticlesSlider, BorderLayout.CENTER);
		numParticles.setToolTipText(readFile("tooltips/NumParticlesSlider.txt"));
		numParticlesSlider.setToolTipText(readFile("tooltips/NumParticlesSlider.txt"));

		numParticles.setBorder(BorderFactory.createBevelBorder(BevelBorder.RAISED));
		// Updates the value displayed by the slider and the text field to that
		// of the simulation
		// Used when "Particles disappear at activation energy" is enabled
		Thread numParticlesUpdater = new Thread(new Runnable() {
			@Override
			public void run() {
				while (true) {
					if (!numParticlesSlider.getValueIsAdjusting() && !numParticlesValue.isFocusOwner()) {
						int num = model.getNumParticles();
						numParticlesSlider.setValue(num);
						numParticlesValue.setText(Integer.toString(num));
					}
					try {
						Thread.sleep(100);
					} catch (InterruptedException e) {
						e.printStackTrace();
					}
				}
			}
		});
		numParticlesUpdater.start();

		// Create the Activation Energy slider and its surrounding components
		JPanel actEnergyText = new JPanel(new GridLayout(0, 1));
		JPanel actEnergyTextTop = new JPanel();
		JPanel actEnergyTextBot = new JPanel();
		actEnergyLabel = new JLabel("Activation Energy (~E-21 J)", SwingConstants.CENTER);
		actEnergyLabel.setFont(new Font("Calibri", Font.PLAIN, 12));
		actEnergyLabel.setEnabled(false);
		actEnergySlider = new JSlider(SwingConstants.HORIZONTAL, 0, 100, 10);
		actEnergySlider.setMajorTickSpacing(20);
		actEnergySlider.setMinorTickSpacing(10);
		actEnergySlider.setPaintTicks(true);
		actEnergySlider.setPaintLabels(true);
		actEnergyValue = new JTextField("10");
		// Make sure only integers can be entered into the text field
		PlainDocument actEnergyDoc = (PlainDocument) actEnergyValue.getDocument();
		actEnergyDoc.setDocumentFilter(new IntFilter());

		actEnergyValue.setColumns(3);
		actEnergyValue.setEnabled(false);
		// When the user presses Enter to confirm their input
		actEnergyValue.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				int val = Integer.parseInt(actEnergyValue.getText());
				int max = actEnergySlider.getMaximum();
				int min = actEnergySlider.getMinimum();
				// Make sure the new value is a valid one
				if (val > max) {
					val = max;
				}
				if (val < min) {
					val = min;
				}
				actEnergySlider.setValue(val);
				actEnergySlider.requestFocus();
				actEnergyValue.setText(Integer.toString(val));
			}
		});
		// Make sure the slider and the text field show the same value
		actEnergySlider.addChangeListener(new ChangeListener() {
			public void stateChanged(ChangeEvent e) {
				model.setActivationEnergy(actEnergySlider.getValue());
				actEnergyValue.setText(Integer.toString(actEnergySlider.getValue()));
			}
		});
		actEnergySlider.setEnabled(false);
		actEnergyTextTop.add(actEnergyLabel);
		actEnergyTextBot.add(actEnergyValue);
		actEnergyText.add(actEnergyTextTop);
		actEnergyText.add(actEnergyTextBot);
		actEnergy.add(actEnergyText, BorderLayout.NORTH);
		actEnergy.add(actEnergySlider, BorderLayout.CENTER);
		actEnergy.setToolTipText(readFile("tooltips/ActEnergySlider.txt"));
		actEnergySlider.setToolTipText(readFile("tooltips/ActEnergySlider.txt"));

		actEnergy.setBorder(BorderFactory.createBevelBorder(BevelBorder.RAISED));

		// Create the Wall Temperature slider and its surrounding components
		JPanel tempText = new JPanel(new GridLayout(0, 1));
		JPanel tempTextTop = new JPanel();
		JPanel tempTextBot = new JPanel();
		tempSlider = new JSlider(SwingConstants.HORIZONTAL, 200, 4000, 300);
		JTextField tempValue = new JTextField("300");
		// Make sure only integers can be entered into the text field
		PlainDocument tempDoc = (PlainDocument) tempValue.getDocument();
		tempDoc.setDocumentFilter(new IntFilter());

		tempValue.setColumns(3);
		// When the user presses Enter to confirm their input
		tempValue.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				int val = Integer.parseInt(tempValue.getText());
				int max = tempSlider.getMaximum();
				int min = tempSlider.getMinimum();
				// Make sure the new value is a valid one
				if (val > max) {
					val = max;
				}
				if (val < min) {
					val = min;
				}
				tempSlider.setValue(val);
				tempSlider.requestFocus();
				tempValue.setText(Integer.toString(val));
			}
		});
		// Make sure the slider and the text field show the same value
		tempSlider.addChangeListener(new ChangeListener() {
			public void stateChanged(ChangeEvent e) {
				model.setT(tempSlider.getValue());
				tempValue.setText(Integer.toString(tempSlider.getValue()));
			}
		});
		JLabel tempLabel = new JLabel("Wall Temperature (K)", SwingConstants.CENTER);
		tempLabel.setFont(new Font("Calibri", Font.PLAIN, 12));
		tempSlider.setMajorTickSpacing(950);
		tempSlider.setMinorTickSpacing(190);
		tempSlider.setPaintTicks(true);
		tempSlider.setPaintLabels(true);
		tempTextTop.add(tempLabel);
		tempTextBot.add(tempValue);
		tempText.add(tempTextTop);
		tempText.add(tempTextBot);
		tempComp.add(tempText, BorderLayout.NORTH);
		tempComp.add(tempSlider, BorderLayout.CENTER);

		tempComp.setToolTipText(readFile("tooltips/WallTempSlider.txt"));
		tempSlider.setToolTipText(readFile("tooltips/WallTempSlider.txt"));

		tempComp.setBorder(BorderFactory.createBevelBorder(BevelBorder.RAISED));

		currT = new JLabel("<html>Average temperature<br>of particles: </html>");
		currT.setFont(new Font("Calibri", Font.PLAIN, 12));
		currT.setToolTipText(readFile("tooltips/AvgTempLabel.txt"));
		currP = new JLabel("<html>Average pressure<br>on container: </html>");
		currP.setFont(new Font("Calibri", Font.PLAIN, 12));
		currP.setToolTipText(readFile("tooltips/AvgPressureLabel.txt"));
		stats.add(currT, BorderLayout.NORTH);
		stats.add(currP, BorderLayout.SOUTH);

		// Create the Simulation Speed Multiplier slider and its surrounding
		// components
		fpsSlider = new JSlider(SwingConstants.HORIZONTAL, 1, 16, 4);
		Hashtable<Integer, JLabel> labelTable = new Hashtable<Integer, JLabel>();
		labelTable.put(Integer.valueOf(2), new JLabel("0.5"));
		labelTable.put(Integer.valueOf(4), new JLabel("1"));
		labelTable.put(Integer.valueOf(6), new JLabel("1.5"));
		labelTable.put(Integer.valueOf(8), new JLabel("2"));
		labelTable.put(Integer.valueOf(12), new JLabel("3"));
		labelTable.put(Integer.valueOf(16), new JLabel("4"));
		fpsSlider.setLabelTable(labelTable);
		JPanel fpsText = new JPanel(new GridLayout(0, 1));
		JPanel fpsTextTop = new JPanel(new FlowLayout(FlowLayout.CENTER, 10, 7));
		JPanel fpsTextBot = new JPanel();
		JLabel fpsLabel = new JLabel("Simulation Speed Multiplier", SwingConstants.CENTER);
		fpsLabel.setFont(new Font("Calibri", Font.PLAIN, 12));
		JLabel fpsValue = new JLabel("1.00");
		fpsValue.setFont(new Font("Calibri", Font.PLAIN, 12));
		// Make sure the slider and the text field show the same value
		fpsSlider.addChangeListener(new ChangeListener() {
			public void stateChanged(ChangeEvent e) {
				comp.setFps(fpsSlider.getValue() * 15);
				fpsValue.setText(String.format("%.2f", fpsSlider.getValue() / 4.0));
			}
		});
		fpsSlider.setMajorTickSpacing(1);
		fpsTextTop.add(fpsLabel);
		fpsTextBot.add(fpsValue);
		fpsText.add(fpsTextTop);
		fpsText.add(fpsTextBot);
		fpsSlider.setPaintTicks(true);
		fpsSlider.setPaintLabels(true);
		fps.add(fpsSlider, BorderLayout.CENTER);
		fps.add(fpsText, BorderLayout.NORTH);

		fps.setToolTipText(readFile("tooltips/SimSpeedSlider.txt"));
		fpsSlider.setToolTipText(readFile("tooltips/SimSpeedSlider.txt"));

		fps.setBorder(BorderFactory.createBevelBorder(BevelBorder.RAISED));

		// Place the components into one panel
		sliderBar.add(stats);
		sliderBar.add(tempComp);
		sliderBar.add(actEnergy);
		sliderBar.add(numParticles);
		sliderBar.add(fps);

		return sliderBar;
	}

	/**
	 * @return A JPanel containing all components on the button bar (bottom
	 *         right)
	 */
	private JPanel buttonBar() {
		JPanel simControls = new JPanel(new GridLayout(1, 3));
		JPanel buttons = new JPanel(new GridLayout(0, 1));

		// Create the Pause / Resume button
		playPause = new JButton(createImageIcon("Pause.png", "Pause icon"));
		playPause.setFont(new Font("Calibri", Font.BOLD, 12));
		// What happens when the button is pressed
		playPause.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				if (pause) {
					model.resumeSim();
					playPause.setIcon(createImageIcon("Pause.png", "Pause icon"));
					pause = false;
				} else {
					model.rollbackBuffer();
					model.pauseSim();
					playPause.setIcon(createImageIcon("Play.png", "Play icon"));
					pause = true;
					comp.stopWalls();
				}
			}
		});
		playPause.setToolTipText(readFile("tooltips/PauseResumeButton.txt"));

		// Create the Restart button
		restart = new JButton("Restart");
		restart.setFont(new Font("Calibri", Font.BOLD, 12));
		// What happens when the button is pressed
		restart.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				model.restartSim();
				comp.stopWalls();
				if (pause) {
					pause = false;
					playPause.setIcon(createImageIcon("Pause.png", "Pause icon"));
				}
				if (!carnotRestart && running) {
					autoCarnot.doClick();
				}
				carnotRestart = false;
			}
		});
		restart.setToolTipText(readFile("tooltips/RestartButton.txt"));

		// Create the Reset button
		JButton reset = new JButton("Reset");
		reset.setFont(new Font("Calibri", Font.BOLD, 12));
		reset.setToolTipText(readFile("tooltips/ResetButton.txt"));
		// What happens when the button is pressed
		reset.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				numParticlesSlider.setValueIsAdjusting(true);
				tempSlider.setValue(300);
				actEnergySlider.setValue(10);
				fpsSlider.setValue(4);

				String m = (String) menu.getSelectedItem();
				if (m.equals("Heat Engines")) {
					colourParticlesAtActEnergy.setSelected(false);
				} else if (m.equals("Activation Energy")) {
					colourParticlesAtActEnergy.setSelected(true);
				}
				particlesDisappearAtActEnergy.setSelected(false);
				particlesPushWall.setSelected(false);

				view.pressRemoveTraces();
				view.bfrClearChart();

				model.getContainer().setWidth(900);
				model.getContainer().setWidthChange(0);
				numParticlesSlider.setValue(250);
				restart.doClick();
				insulated.setSelected(false);
				numParticlesSlider.setValueIsAdjusting(false);
			}
		});

		simControls.add(restart);
		simControls.add(playPause);
		simControls.add(reset);

		JPanel moveWall = new JPanel(new GridLayout(1, 1));

		// Create the Move Wall In button
		moveWallIn = new JButton("Move Wall In");
		moveWallIn.setFont(new Font("Calibri", Font.BOLD, 12));
		// What happens when the button is pressed
		moveWallIn.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				// Don't do anything if the sim is paused
				if (!pause) {
					comp.moveWallInAuto(2, moveWallIn);
				}
			}
		});
		moveWallIn.setToolTipText(readFile("tooltips/MoveWallInButton.txt"));

		// Create the Move Wall Out button
		JButton moveWallOut = new JButton("Move Wall Out");
		moveWallOut.setFont(new Font("Calibri", Font.BOLD, 12));
		// What happens when the button is pressed
		moveWallOut.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				// Don't do anything if the sim is paused
				if (!pause) {
					comp.moveWallOutAuto(2, moveWallOut);
				}
			}
		});
		moveWallOut.setToolTipText(readFile("tooltips/MoveWallOutButton.txt"));

		moveWall.add(moveWallIn);
		moveWall.add(moveWallOut);

		// Create the "Insulate the walls" checkbox
		insulated = new JCheckBox("Insulate the walls");
		insulated.addItemListener(new ItemListener() {
			@Override
			public void itemStateChanged(ItemEvent e) {
				view.pvAddTrace();
				view.tsAddTrace();
				if (e.getStateChange() == ItemEvent.DESELECTED) {
					model.setIsInsulated(false);
				} else {
					model.setIsInsulated(true);
				}
			}
		});
		insulated.setToolTipText(readFile("tooltips/InsulateWallsCheckbox.txt"));

		// Create the "Colour particles at activation energy" checkbox
		colourParticlesAtActEnergy = new JCheckBox("Colour particles at activation energy");
		colourParticlesAtActEnergy.setFont(new Font("Calibri", Font.PLAIN, 11));
		colourParticlesAtActEnergy.addItemListener(new ItemListener() {
			@Override
			public void itemStateChanged(ItemEvent e) {
				if (e.getStateChange() == ItemEvent.DESELECTED) {
					comp.setColouringParticles(false);
				} else {
					comp.setColouringParticles(true);
				}
			}
		});
		colourParticlesAtActEnergy.setToolTipText(readFile("tooltips/ColourParticlesCheckbox.txt"));
		colourParticlesAtActEnergy.setEnabled(false);

		// Create the "Make particles disappear at activation energy" checkbox
		particlesDisappearAtActEnergy = new JCheckBox("Make particles disappear at activation energy");
		particlesDisappearAtActEnergy.setFont(new Font("Calibri", Font.PLAIN, 11));
		particlesDisappearAtActEnergy.addItemListener(new ItemListener() {
			@Override
			public void itemStateChanged(ItemEvent e) {
				if (e.getStateChange() == ItemEvent.DESELECTED) {
					model.setDisappearOnActEnergy(false);
				} else {
					model.setDisappearOnActEnergy(true);
				}
			}
		});
		particlesDisappearAtActEnergy.setToolTipText(readFile("tooltips/ParticlesDisappearCheckbox.txt"));
		particlesDisappearAtActEnergy.setEnabled(false);

		// Create the "Allow particles to push the right wall" checkbox
		particlesPushWall = new JCheckBox("Allow particles to push the right wall");
		particlesPushWall.addItemListener(new ItemListener() {
			@Override
			public void itemStateChanged(ItemEvent e) {
				if (e.getStateChange() == ItemEvent.DESELECTED) {
					model.setParticlesPushWall(false);
					model.getContainer().setWidthChange(0);
				} else {
					model.setParticlesPushWall(true);
				}
			}
		});
		particlesPushWall.setToolTipText(readFile("tooltips/ParticlesPushCheckbox.txt"));

		// Place all the components into one panel
		buttons.add(simControls);
		buttons.add(moveWall);
		buttons.add(insulated);
		buttons.add(colourParticlesAtActEnergy);
		buttons.add(particlesDisappearAtActEnergy);
		buttons.add(particlesPushWall);
		return buttons;
	}

	/**
	 * Create the buttons used to automatically create a Carnot cycle.
	 */
	private void createAutoCarnot() {
		autoCarnot = new JButton("Single Carnot");
		autoCarnot.setFont(new Font(null, Font.BOLD, 10));
		autoCarnot.setToolTipText(readFile("tooltips/AutoCarnotButton.txt"));
		autoCarnot.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				autoCarnot(false);
			}
		});

		autoCarnotCont = new JButton("Continual Carnot");
		autoCarnotCont.setFont(new Font(null, Font.BOLD, 10));
		autoCarnotCont.setToolTipText(readFile("tooltips/AutoCarnotConstButton.txt"));
		autoCarnotCont.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent e) {
				autoCarnot(true);
			}
		});
	}

	/**
	 * Creates one or more Carnot cycles using the tools in the program.
	 * 
	 * @param continuous
	 *            Do we want to produce more than one cycle?
	 */
	private void autoCarnot(boolean continuous) {
		Container cont = model.getContainer();
		Thread t = new Thread(new Runnable() {
			public void run() {
				running = true;
				autoCarnot.setText("Stop Carnot");
				autoCarnotCont.setText("Stop Carnot");
				// Number of times we've been through the loop
				int i = 0;
				while (running && ((i == 0 && !continuous) || continuous)) {
					// Insulation off, allow gas to move wall out, wall
					// starts in until halfway, high wall temp --> high
					// wall temp
					if (i == 0) {
						playPause.doClick(50);
						fpsSlider.setValue(4);
					}
					comp.stopWalls();
					cont.setWidth(cont.getMinWidth());
					tempSlider.setValue(3000);
					if (i == 0) {
						model.setNumParticles(250);
						carnotRestart = true;
						restart.doClick(50);
						view.pressRemoveTraces();
					}
					insulated.setSelected(false);
					particlesPushWall.setSelected(true);

					// Wait until the wall is pushed to 2/3 of the way to the
					// maximum container width
					double contTwoThirds = cont.getMinWidth() + 2 * (cont.getMaxWidth() - cont.getMinWidth()) / 3;
					while (cont.getWidth() < contTwoThirds && running) {
						try {
							Thread.sleep(50);
						} catch (InterruptedException e) {
							e.printStackTrace();
						}
					}
					if (!running) {
						return;
					}

					// Insulation on, allow gas to move wall out, wall
					// starts halfway until out, high wall temp --> low
					// wall temp
					insulated.setSelected(true);

					// Wait until the wall is pushed to the maximum container
					// width
					while (cont.getWidth() < cont.getMaxWidth() && running) {
						try {
							Thread.sleep(50);
						} catch (InterruptedException e) {
							e.printStackTrace();
						}
					}
					if (!running) {
						return;
					}

					// Insulation off, compress gas, wall starts out
					// until halfway, low wall temp --> low wall temp
					comp.stopWalls();
					insulated.setSelected(false);
					particlesPushWall.setSelected(false);
					cont.setWidth(cont.getMaxWidth());
					tempSlider.setValue(1000);
					moveWallIn.doClick(50);

					// Wait until the wall moves to 1/3 of the way to the
					// minimum container width
					double contOneThird = cont.getMinWidth() + (cont.getMaxWidth() - cont.getMinWidth()) / 3;
					while (cont.getWidth() > contOneThird && running) {
						try {
							Thread.sleep(50);
						} catch (InterruptedException e) {
							e.printStackTrace();
						}
					}
					if (!running) {
						return;
					}

					// Insulation on, compress gas, wall starts halfway
					// until in, low wall temp --> high wall temp
					insulated.setSelected(true);

					// Wait until the wall moves to the minimum container width
					while (cont.getWidth() > cont.getMinWidth() && running) {
						try {
							Thread.sleep(50);
						} catch (InterruptedException e) {
							e.printStackTrace();
						}
					}
					if (!running) {
						return;
					}
					i++;
				}
				playPause.doClick(50);
				running = false;
				autoCarnot.setText("Single Carnot");
				autoCarnotCont.setText("Continual Carnot");
			}
		});
		if (running) {
			running = false;
			particlesPushWall.setSelected(false);
			autoCarnot.setText("Single Carnot");
			autoCarnotCont.setText("Continual Carnot");
		} else {
			t.start();
		}
	}

	/**
	 * @return Is the simulation paused?
	 */
	public boolean isPaused() {
		return pause;
	}

	/**
	 * Reads a text file with name name and returns a String with its contents.
	 * 
	 * @param name
	 *            The name of the text file
	 * @return The contents of the text file
	 */
	public String readFile(String name) {
		String s = "";
		URL url = this.getClass().getResource("/resources/" + name);
		InputStream stream = null;
		try {
			stream = url.openStream();
		} catch (IOException e) {
			e.printStackTrace();
		}
		ByteArrayOutputStream result = new ByteArrayOutputStream();
		byte[] buffer = new byte[1024];
		int length;
		try {
			while ((length = stream.read(buffer)) != -1) {
				result.write(buffer, 0, length);
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
		try {
			return result.toString("UTF-8");
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
		}
		try {
			stream.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
		return s;
	}

	/**
	 * Opens an image and returns an ImageIcon, or null if the path was invalid.
	 * 
	 * @param name
	 *            The name to the image
	 * @param description
	 *            A description of the image
	 * @return An ImageIcon of the specified image
	 */
	public ImageIcon createImageIcon(String name, String description) {
		URL imgURL = this.getClass().getResource("/resources/images/" + name);
		if (imgURL != null) {
			return new ImageIcon(imgURL, description);
		} else {
			System.err.println("Couldn't find file: /resources/images/" + name);
			return null;
		}
	}
}
